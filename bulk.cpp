#include <iostream>
#include <fstream>
#include <sstream>

#include <map>

#include "bulk_internal.h"

namespace otus_hw7{
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

    bool     CommandQueue::pop(ICommandPtr_t& cmd) 
    {
        if( q_.empty() )
            return false;
        cmd = std::move(q_.front());
        q_.pop();
        return true;
    }

    void QueueExecutor::execute(ICommandQueue& q, ICommandExecutor& cmd_executor, ICommandContext& ctx)
    {
        ICommandPtr_t cmd;
        for( ; q.pop(cmd) ; ++ctx.cmd_idx_)
        {
            cmd_executor.execute_cmd(*cmd, ctx); 
        } 
    }

    void SimpleCommand::execute(ICommandContext& ctx)
    {
        ctx.os_ << (!ctx.cmd_idx_ ? "bulk: ": ", ") << cmd_; 
        if( ctx.bulk_size_ - ctx.cmd_idx_ < 2 )
            ctx.os_ << std::endl; 
    }

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