#include <iostream>
#include <map>

#include "bulk.h"

namespace otus_hw7{
    using command_data_t = std::string;
    class SimpleCommand : public ICommand
    {
    public:
        SimpleCommand(const command_data_t& cmd) : cmd_(cmd) {}
        virtual void execute(ICommandContext& ctx) override;
    private:
        command_data_t cmd_;
    };

    class InputParser : public IInputParser
    {
    public:
        InputParser(size_t chunk_size, istream& is) : is_(is), chunk_size_(chunk_size), last_tok_{}, last_stat_{} { }
        Status   read_next_command(ICommandPtr_t& cmd) override
        {
            read_command();
            command_data_t cmd_data;
            Status st = get_last_command_data(cmd_data);
            cmd = create_command(cmd_data);
            return st;
        }
        
        Status   read_next_bulk(ICommandQueue& cmd_queue) override;         

    private:
        enum class Token : uint8_t
        {
            kCommand,
            kBegin_Block,
            kEnd_Block,
            kEnd_Of_File
        };

        istream&   read_command();
        Status     get_last_command_data(std::string& cmd) const { cmd = last_cmd_; return last_stat_; }
        void       set_status(Status new_st);
        ICommandPtr_t create_command(const std::string cmd) const { return ICommandPtr_t{new SimpleCommand(cmd)}; }
        istream&   is_;
        size_t     chunk_size_, cmd_count_ = 0, block_count_ = 0;
        ICommandPtr_t  p_last_cmd_; 

        command_data_t  last_cmd_; 
        Token        last_tok_;       
        Status       last_stat_;       
    };

    IInputParser::Status   InputParser::read_next_bulk(ICommandQueue& cmd_queue)
    {
		Status st{};
        for(bool end_of_work = false; !end_of_work;)
        {
            ICommandPtr_t cmd;
			switch( st = read_next_command(cmd) )
			{
				default:
				case Status::kIgnore:
					break;
				case Status::kReading:
					cmd_queue.push(std::move(cmd));
					break;
				case Status::kReady:
                    end_of_work = true;
                    break;
				case Status::kStop:
                    end_of_work = true;
					cmd_queue.reset();
					break;
			}
		}
        return st;
    }    

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
        void     push(ICommandPtr_t cmd) override { q_.push(std::move(cmd)); }
        bool     pop(ICommandPtr_t& cmd) override;
        void     reset() override { while(!q_.empty()) q_.pop(); }
        size_t   size() const override { return q_.size(); }

    private:
        using  queue_t = std::queue<ICommandPtr_t>;
        queue_t q_;
    };
   
    bool     CommandQueue::pop(ICommandPtr_t& cmd) 
    {
        if( q_.empty() )
            return false;
        cmd = std::move(q_.front());
        q_.pop();
        return true;
    }

    class QueueExecutor : public IQueueExecutor
    {
    public:    
        QueueExecutor()  { }
        void execute(ICommandQueue& q, ICommandContext& ctx) override;
    };

    void QueueExecutor::execute(ICommandQueue& q, ICommandContext& ctx)
    {
        ICommandPtr_t cmd;
        for( ; q.pop(cmd) ; ++ctx.cmd_idx_)
        {
            cmd->execute(ctx);
        } 
    }

    void SimpleCommand::execute(ICommandContext& ctx)
    {
        ctx.os_ << (!ctx.cmd_idx_ ? "bulk: ": ", ") << cmd_; 
        if( ctx.bulk_cnt_ - ctx.cmd_idx_ < 2 )
            ctx.os_ << std::endl; 
    }

    class Processor : public IProcessor
    {
    public:
        Processor(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, IQueueExecutorPtr_t executor) :
        parser_(std::move(parser)), cmd_queue_(std::move(cmd_queue)), executor_(std::move(executor)), 
        ctx_(std::make_unique<ICommandContext>(0, 0, std::cout)) {}
        void process() override;
    private:
        void     exec_queue( );

        IInputParserPtr_t parser_;
        ICommandQueuePtr_t cmd_queue_; 
        IQueueExecutorPtr_t executor_;
        ICommandContextPtr_t ctx_;
    };

    void Processor::process()
    {
		for(bool end_of_work = false; !end_of_work;)
        {
			IInputParser::Status st = parser_->read_next_bulk(*cmd_queue_);
			switch( st )
			{
				default:
				case IInputParser::Status::kIgnore:
					break;
				case IInputParser::Status::kReading:
					break;
				case IInputParser::Status::kReady:
					exec_queue();
					break;
				case IInputParser::Status::kStop:
                    end_of_work = true;
					cmd_queue_->reset();
					break;
			}
		}
    }

    void    Processor::exec_queue( )
    {
        ctx_->bulk_cnt_ = cmd_queue_->size(); 
        ctx_->cmd_idx_ = 0;
        
        executor_->execute(*cmd_queue_, *ctx_);
    }

    IProcessorPtr_t create_processor(Options& options)
    {
        return IProcessorPtr_t{new Processor(create_parser(options), create_command_queue(), create_queue_executor() ) };
    }
    
    IInputParserPtr_t create_parser(Options& options)
    {
        return IInputParserPtr_t{ new InputParser(options.cmd_chunk_sz, std::cin) };
    }
    
    ICommandQueuePtr_t create_command_queue()
    {
        return ICommandQueuePtr_t{ new CommandQueue };
    }

    IQueueExecutorPtr_t create_queue_executor()
    {
        return IQueueExecutorPtr_t{ new QueueExecutor() };
    }
};