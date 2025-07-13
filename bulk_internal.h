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

    /// @brief Реализация парсера входного потока команд
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

    /// @brief Реализация очереди команд
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

    /// @brief Декоратор для исполнителя очереди
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

    /// @brief Декоратор для исполнителя команд
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

    /// @brief Реализация декоратора для сохранения команды в файл
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

    /// @brief Реализация процессора команд
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

    /// @brief Фабрика для парсера. Опции нужны для выбора типа парсера
    /// @param options 
    /// @return 
    IInputParserPtr_t create_parser(Options& options);
    
    /// @brief Фабрика очереди команд
    /// @return Указатель на абстрактный интерфейс очереди команд 
    ICommandQueuePtr_t create_command_queue();

    /// @brief Фабрика исполнителя очереди команд
    /// @return Указатель на созданный интерфейс
    IQueueExecutorPtr_t create_queue_executor();

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options& options);
   
};