#include <iostream>
#include <boost/program_options.hpp>
#include "bulk.h"

namespace otus_hw7{
    namespace po = boost::program_options;
   
    void show_help(po::options_description const& desc)
    {
        std::cout << desc << std::endl;
    }

    bool parse_command_line(int argc, const char* argv[], Options& parsed_options)
    {
        constexpr const char* const OPTION_NAME_HELP = "help"; 
        constexpr const char* const OPTION_NAME_CHUNK_SIZE = "chunk_size"; 
        parsed_options = {false, 0};
        
        auto check_size = [](const size_t& sz) 
                          { 
                            if( sz < 1 ) throw po::invalid_option_value(OPTION_NAME_CHUNK_SIZE); 
                          };
        po::options_description desc("Аргументы командной строки");
        desc.add_options()
            (OPTION_NAME_HELP, po::bool_switch(&parsed_options.show_help), "Отображение справки")
            (OPTION_NAME_CHUNK_SIZE, po::value<size_t>(&parsed_options.cmd_chunk_sz)->notifier(check_size), "Размер блока команд");

        po::positional_options_description pos_desc;
        pos_desc.add(OPTION_NAME_CHUNK_SIZE, -1);

        po::variables_map vm;
        po::store(po::command_line_parser(argc, argv).options(desc).positional(pos_desc).run(), vm);
        po::notify(vm);

        size_t sz = vm.size();
        bool not_need_exit = true;
        if( sz < 2 || !vm.count(OPTION_NAME_CHUNK_SIZE) )
            parsed_options.show_help = true, 
            not_need_exit = false;
        
        if( parsed_options.show_help )
            show_help(desc);
        
        return not_need_exit;
    }

    class InputParser : public IInputParser
    {
    public:
        InputParser(size_t chunk_size, istream& is) : is_(is), chunk_size_(chunk_size), last_tok_{}, last_stat_{} { }
        Status   read_next_command(command_t& cmd) override
        {
            read_command();
            return get_last_command(cmd);
        }

    private:
        enum class Token : uint8_t
        {
            kCommand,
            kBegin_Block,
            kEnd_Block,
            kEnd_Of_File
        };

        istream&   read_command();
        Status     get_last_command(command_t& cmd) const { cmd = last_cmd_; return last_stat_; }
        void       set_status(Status new_st);
        istream&   is_;
        size_t     chunk_size_, cmd_count_ = 0, block_count_ = 0;
        command_t  last_cmd_; 
        Token      last_tok_;       
        Status     last_stat_;       
    };

    void     InputParser::set_status(Status new_st)
    {
        if( new_st == last_stat_ )
            return;

        switch(new_st)
        {
            default: return;
            case InputParser::Status::kIgnore:
            case InputParser::Status::kReading:
    			break;
	
            case InputParser::Status::kReady:
                cmd_count_ = 0;
				break;

            case InputParser::Status::kStop:
                cmd_count_ = 0;
                break;            
        }
        last_stat_ = new_st;
    }

    istream& InputParser::read_command()
    {
        using token_map_t = std::map<std::string, Token>;
        static token_map_t tok_values = {{"{", Token::kBegin_Block}, {"}", Token::kEnd_Block}};

        if( cmd_count_ == chunk_size_ && !block_count_ )
        {
            last_tok_ = Token::kEnd_Block;
            set_status(Status::kReady);
            return is_; 
        }

        std::string inp_str;
        if( !std::getline(is_, inp_str) )
        {
            last_tok_ = block_count_ ? Token::kEnd_Of_File : Token::kEnd_Block;
            set_status(block_count_ || (Status::kReady == last_stat_) ? Status::kStop : Status::kReady);
        }
        else
        {
            last_tok_ = Token::kCommand; 
            auto const p_tok = tok_values.find(inp_str);
            if( p_tok != tok_values.end() )
                last_tok_ = p_tok->second;

            switch( last_tok_ )
            {
                default:
                case Token::kCommand:
                    ++cmd_count_;
                    last_cmd_ = inp_str;
                    set_status(Status::kReading);
                    break;

                case Token::kBegin_Block:
                    if( !block_count_++ )
                        set_status(Status::kReady);
                    else
                        set_status(Status::kIgnore);
                    break;

                case Token::kEnd_Block:
                    if( block_count_ > 0 )
                    {
                        if( !--block_count_ )
                            set_status(Status::kReady);
                        else
                            set_status(Status::kIgnore);
                    }
                    break;
            }
        }
        return is_;
    }

    class CommandQueue : public ICommandQueue
    {
    public:
        void     push(command_t const& cmd) override { q_.push(cmd); }
        void     reset() override { while(!q_.empty()) q_.pop(); }
        void     exec( ICommandExecutor& executor ) override;
    private:
        using  queue_t = std::queue<command_t>;
        queue_t q_;
        ICommandContext ctx_;
    };
   
    void     CommandQueue::exec(ICommandExecutor& executor)
    {
        ctx_ = { q_.size(), 0 };
        for( ; !q_.empty() ; ++ctx_.cmd_idx_)
        {
            command_t cmd = q_.front();
            q_.pop();
            executor.execute_cmd(cmd, ctx_);
        }    
    }

    class CommandExecutor : public ICommandExecutor
    {
    public:    
        CommandExecutor(ostream& os) : os_(os) { }
        void execute_cmd(const command_t& cmd, ICommandContext& ctx) override;
    private:
        ostream& os_;
    };

    void CommandExecutor::execute_cmd(const command_t& cmd, ICommandContext& ctx)
    {
        os_ << (!ctx.cmd_idx_ ? "bulk: ": ", ") << cmd; 
        if( ctx.bulk_cnt_ - ctx.cmd_idx_ < 2 )
            os_ << std::endl; 
    }

    class Processor : public IProcessor
    {
    public:
        Processor(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, ICommandExecutorPtr_t executor) :
        parser_(std::move(parser)), cmd_queue_(std::move(cmd_queue)), executor_(std::move(executor)){}
        void process() override;
    private:
        IInputParserPtr_t parser_;
        ICommandQueuePtr_t cmd_queue_; 
        ICommandExecutorPtr_t executor_;
    };

    void Processor::process()
    {
		for(bool end_of_work = false; !end_of_work;)
        {
            command_t cmd;
			IInputParser::Status st = parser_->read_next_command(cmd);
			switch( st )
			{
				default:
				case IInputParser::Status::kIgnore:
					break;
				case IInputParser::Status::kReading:
					cmd_queue_->push(cmd);
					break;
				case IInputParser::Status::kReady:
					cmd_queue_->exec(*executor_);
					break;
				case IInputParser::Status::kStop:
                    end_of_work = true;
					cmd_queue_->reset();
					break;
			}
		}
    }


    IProcessorPtr_t create_processor(Options& options)
    {
        return IProcessorPtr_t{new Processor(create_parser(options), create_command_queue(), create_command_executor() ) };
    }
    
    IInputParserPtr_t create_parser(Options& options)
    {
        return IInputParserPtr_t{ new InputParser(options.cmd_chunk_sz, std::cin) };
    }
    
    ICommandQueuePtr_t create_command_queue()
    {
        return ICommandQueuePtr_t{new CommandQueue};
    }

    ICommandExecutorPtr_t create_command_executor()
    {
        return ICommandExecutorPtr_t{ new CommandExecutor(std::cout) };
    }
};