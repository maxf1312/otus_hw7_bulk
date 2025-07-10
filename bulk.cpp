#include <iostream>
#include <fstream>
#include <sstream>

#include <map>


#include "bulk.h"

namespace otus_hw7{

    using command_data_t = std::string;

    /// @brief Абстрактная фабрика для команды
    struct ICommandCreator
    {
        virtual ~ICommandCreator() = default;
        virtual ICommandPtr_t create_command(const command_data_t& cmd_data) const = 0;
    };
    using ICommandCreatorPtr_t = std::unique_ptr<ICommandCreator>;

    class InputParser : public IInputParser
    {
    public:
        InputParser(size_t chunk_size, istream& is, ICommandCreatorPtr_t cmd_creator) 
            : is_(is), chunk_size_(chunk_size), cmd_creator_{std::move(cmd_creator)}, last_tok_{}, last_stat_{} { }
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
        ICommandPtr_t create_command(const command_data_t&  cmd) const { return cmd_creator_->create_command(cmd); }
        istream&   is_;
        size_t     chunk_size_, cmd_count_ = 0, block_count_ = 0;

        ICommandCreatorPtr_t cmd_creator_;
        command_data_t  last_cmd_; 
        Token        last_tok_;       
        Status       last_stat_;       
    };

    /// @brief Реализация простой команды, выводящей себя в поток
    class SimpleCommand : public ICommand
    {
    public:
        SimpleCommand(const command_data_t& cmd) : cmd_(cmd) {}
        virtual void execute(ICommandContext& ctx) override;
    private:
        command_data_t cmd_;
    };

    /// @brief Реализация команды-декоратора
    class CommandDecorator : public ICommand
    {
    public:
        CommandDecorator(ICommandPtr_t wraped_cmd) : wrapped_cmd_(std::move(wraped_cmd)) {}
        virtual void execute(ICommandContext& ctx) override 
        {
            wrapped_cmd_->execute(ctx);
        }
    private:
        ICommandPtr_t wrapped_cmd_;
    };

    /// @brief Конкретная фабрика команд
    class CommandCreator : public ICommandCreator
    {
    public:
        virtual ICommandPtr_t create_command(const command_data_t&  cmd) const override 
        { 
            return ICommandPtr_t{new SimpleCommand(cmd)}; 
        }
    };

    IInputParser::Status   InputParser::read_next_bulk(ICommandQueue& cmd_queue)
    {
		Status st{};
        if( !cmd_queue.size() )
            cmd_queue.created_at_ = std::time(nullptr); 

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

    /// @brief Реализация исполнителя очереди
    class QueueExecutor : public IQueueExecutor
    {
    public:    
        QueueExecutor()  { }
        virtual void execute(ICommandQueue& q, ICommandExecutor& cmd_executor, ICommandContext& ctx) override;
    };

    /// @brief Реализация исполнителя команд
    class CommandExecutor : public ICommandExecutor
    {
    public:    
        CommandExecutor()  { }
        virtual void execute_cmd(ICommand& cmd, ICommandContext& ctx) override
        {
            cmd.execute(ctx);
        }
    };

    void QueueExecutor::execute(ICommandQueue& q, ICommandExecutor& cmd_executor, ICommandContext& ctx)
    {
        ICommandPtr_t cmd;
        for( ; q.pop(cmd) ; ++ctx.cmd_idx_)
        {
            cmd_executor.execute_cmd(*cmd, ctx); 
        } 
    }

    class QueueExecutorDecorator : public IQueueExecutor
    {
    public:
        QueueExecutorDecorator(IQueueExecutorPtr_t wrapee) : wrapee_(std::move(wrapee)) {}
        virtual void execute(ICommandQueue& q, ICommandExecutor& cmd_executor, ICommandContext& ctx) override 
        {
            wrapee_->execute(q, cmd_executor, ctx);
        }
    private:
        IQueueExecutorPtr_t wrapee_;
    };

    class CommandExecutorDecorator : public ICommandExecutor
    {
    public:
        CommandExecutorDecorator(ICommandExecutorPtr_t wrapee) : wrapee_(std::move(wrapee)) {}
        virtual void execute_cmd(ICommand& c, ICommandContext& ctx) override 
        {
            wrapee_->execute_cmd(c, ctx);
        }
    private:
        ICommandExecutorPtr_t wrapee_;
    };

    /// @brief Реализация декоратора для сохранения времени создания команды
    class CommandExecutorWithLog : public CommandExecutorDecorator 
    {
    public:
        CommandExecutorWithLog(ICommandExecutorPtr_t wrapee) : CommandExecutorDecorator(std::move(wrapee)) {}
        virtual void execute_cmd(ICommand& c, ICommandContext& ctx) override 
        {
            CommandExecutorDecorator::execute_cmd(c, ctx);
            init_log(ctx);
            ICommandContext log_ctx{ctx.bulk_size_, ctx.cmd_idx_, log_, ctx.cmd_created_at_};
            c.execute(log_ctx);
        }
    private:
        std::string get_log_filenm(ICommandContext const& ctx)
        {
            std::ostringstream oss;
            oss << ctx.cmd_created_at_ << ".log";
            return oss.str();
        }

        void init_log(ICommandContext const& ctx)
        {
            if( log_.is_open() )    return;
            std::string file_nm = get_log_filenm(ctx);
            log_.open(file_nm, std::ios_base::out | std::ios_base::ate );
        }

        std::ofstream log_;
    };


    void SimpleCommand::execute(ICommandContext& ctx)
    {
        ctx.os_ << (!ctx.cmd_idx_ ? "bulk: ": ", ") << cmd_; 
        if( ctx.bulk_size_ - ctx.cmd_idx_ < 2 )
            ctx.os_ << std::endl; 
    }

    class Processor : public IProcessor
    {
    public:
        Processor(IInputParserPtr_t parser, ICommandQueuePtr_t cmd_queue, IQueueExecutorPtr_t executor) :
        parser_(std::move(parser)), cmd_queue_(std::move(cmd_queue)), executor_(std::move(executor)), 
        ctx_(std::make_unique<ICommandContext>(0, 0, std::cout, 0)) {}
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
        ctx_->bulk_size_ = cmd_queue_->size(); 
        ctx_->cmd_idx_ = 0;
        ctx_->cmd_created_at_ = cmd_queue_->created_at_;
        ICommandExecutorPtr_t cmd_executor{ new CommandExecutorWithLog(ICommandExecutorPtr_t{ new CommandExecutor() }) };
        executor_->execute(*cmd_queue_, *cmd_executor, *ctx_);
    }

    /// @brief Фабрика для парсера. Опции нужны для выбора типа парсера
    /// @param options 
    /// @return 
    IInputParserPtr_t create_parser(Options& options)
    {
        return IInputParserPtr_t{ new InputParser(options.cmd_chunk_sz, std::cin, ICommandCreatorPtr_t(new CommandCreator)) };
    }
    
    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue()
    {
        return ICommandQueuePtr_t{ new CommandQueue };
    }

    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor()
    {
        return  IQueueExecutorPtr_t{  new QueueExecutor() };
    }

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options& options)
    {
        return IProcessorPtr_t{new Processor(create_parser(options), create_command_queue(), create_queue_executor() ) };
    }
    
};