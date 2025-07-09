#pragma once

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
    struct ICommandContext;
    struct IInputParser;
    struct ICommand;
    struct ICommandQueue;
    struct IProcessor;

    using IQueueExecutorPtr_t = std::unique_ptr<IQueueExecutor>;
    using IInputParserPtr_t = std::unique_ptr<IInputParser>;
    using ICommandPtr_t = std::unique_ptr<ICommand>;
    using ICommandQueuePtr_t = std::unique_ptr<ICommandQueue>;
    using IProcessorPtr_t = std::unique_ptr<IProcessor>;
    using ICommandContextPtr_t = std::unique_ptr<ICommandContext>;

    /**
         * @brief Интерфейс для разбора ввода и формирования пакетов команд. 
         *        Формируут пакеты, возвращая сразу данные в ICommandQueue 
         *          
         */
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

    struct ICommandQueue
    {
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

    /// @brief Актор, выполняющий очередь
    struct IQueueExecutor
    {
        virtual      ~IQueueExecutor() = default;
        virtual void execute(ICommandQueue& cmd_q, ICommandContext& ctx) = 0;
    };

    /// @brief Контекст выполнения команды
    struct ICommandContext
    {
        size_t bulk_cnt_, cmd_idx_;
        ostream& os_;

        virtual ~ICommandContext() = default; 
        ICommandContext(size_t bulk_cnt, size_t cmd_idx, ostream& os) 
            : bulk_cnt_(bulk_cnt), cmd_idx_(cmd_idx), os_(os) {}
    };

    struct IProcessor
    {
        virtual ~IProcessor() = default;
        virtual void process() = 0;
    };

    IProcessorPtr_t create_processor(Options& options);
    IInputParserPtr_t create_parser(Options& options);
    ICommandQueuePtr_t create_command_queue();
    IQueueExecutorPtr_t create_queue_executor();
} // otus_hw7

