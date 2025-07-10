#pragma once

#include <ctime>
#include <iostream>
#include <string>
#include <memory>
#include <queue>


namespace otus_hw7{
    using std::istream;
    using std::ostream;
    struct Options
    {
        bool   show_help;
        size_t cmd_chunk_sz;
    };
    bool parse_command_line(int argc, const char* argv[], Options& parsed_options);


    struct IQueueExecutor;
    struct ICommandExecutor;
    struct ICommandContext;
    struct IInputParser;
    struct ICommand;
    struct ICommandQueue;
    struct IProcessor;

    using IQueueExecutorPtr_t = std::unique_ptr<IQueueExecutor>;
    using ICommandExecutorPtr_t = std::unique_ptr<ICommandExecutor>;
    using IInputParserPtr_t = std::unique_ptr<IInputParser>;
    using ICommandPtr_t = std::unique_ptr<ICommand>;
    using ICommandQueuePtr_t = std::unique_ptr<ICommandQueue>;
    using IProcessorPtr_t = std::unique_ptr<IProcessor>;
    using ICommandContextPtr_t = std::unique_ptr<ICommandContext>;

    //---------------------------------------------------------------------------------------------------
    
    /// @brief  Парсер для четния, разбора ввода и формирования пакетов команд. 
    ///         Формирует пакеты, возвращая сразу данные в ICommandQueue 
    struct IInputParser
    {
        /// @brief Статус чтения ввода и готовности к выполнению
        enum class Status : uint8_t
        {
            kReading,
            kReady,
            kIgnore,
            kStop
        };
        virtual          ~IInputParser() = default;
        virtual Status   read_next_command(ICommandPtr_t& cmd) = 0;
        virtual Status   read_next_bulk(ICommandQueue& cmd_queue) = 0;        
    };

    /// @brief Очередь команд. Формируется парсером, затем выполняется исполнителем под управлением процессора.
    struct ICommandQueue
    {
        time_t   created_at_;
        virtual  ~ICommandQueue() = default;

        virtual  void push(ICommandPtr_t cmd) = 0;
        virtual  bool pop(ICommandPtr_t& cmd) = 0;
        virtual  void reset() = 0;
        virtual  size_t size() const = 0;
    };

    /// @brief Команда, активный объект, паттерн команда
    struct ICommand
    {
        virtual      ~ICommand() = default;
        virtual void execute(ICommandContext& ctx) = 0;
    };

    /// @brief Актор, выполняющий команду
    struct ICommandExecutor
    {
        virtual      ~ICommandExecutor() = default;
        virtual void execute_cmd(ICommand& cmd, ICommandContext& ctx) = 0;
    };

    /// @brief Актор, выполняющий очередь
    struct IQueueExecutor
    {
        virtual      ~IQueueExecutor() = default;
        virtual void execute(ICommandQueue& cmd_q, ICommandExecutor& cmd_executor, ICommandContext& ctx) = 0;
    };

    /// @brief Контекст выполнения команды
    struct ICommandContext
    {
        size_t bulk_size_, cmd_idx_;
        ostream& os_;
        time_t cmd_created_at_;

        virtual ~ICommandContext() = default; 
        ICommandContext(size_t bulk_size, size_t cmd_idx, ostream& os) 
            : bulk_size_(bulk_size), cmd_idx_(cmd_idx), os_(os), cmd_created_at_(0) {}
    };

    /// @brief Процессор - управляющий обработкой посредник
    struct IProcessor
    {
        virtual ~IProcessor() = default;
        virtual void process() = 0;
    };

    /// @brief  Фабрика для процессора, сама по настройкам выбирает какой тип процессора создать
    /// @param options 
    /// @return Интерфейс созданного объекта  
    IProcessorPtr_t create_processor(Options& options);
} // otus_hw7

