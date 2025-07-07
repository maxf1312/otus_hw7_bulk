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

    using command_t = std::string;

    struct ICommandExecutor;
    struct ICommandContext;
    struct IInputParser;
    struct ICommandQueue;
    struct IProcessor;

    using ICommandExecutorPtr_t = std::unique_ptr<ICommandExecutor>;
    using IInputParserPtr_t = std::unique_ptr<IInputParser>;
    using ICommandQueuePtr_t = std::unique_ptr<ICommandQueue>;
    using IProcessorPtr_t = std::unique_ptr<IProcessor>;

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
        virtual Status   read_next_command(command_t& cmd) = 0;
    };

    struct ICommandQueue
    {
        virtual  ~ICommandQueue() = default;

        virtual  void push(command_t const& cmd) = 0;
        virtual  void reset() = 0;
        virtual  void exec( ICommandExecutor& executor ) = 0;
    };

    /// @brief Актор, выполняющий команду из очереди
    struct ICommandExecutor
    {
        virtual      ~ICommandExecutor() = default;
        virtual void execute_cmd(const command_t& cmd, ICommandContext& ctx) = 0;
    };

    /// @brief Контекст выполнения команды
    struct ICommandContext
    {
        size_t bulk_cnt_, cmd_idx_;
    };

    struct IProcessor
    {
        virtual ~IProcessor() = default;
        virtual void process() = 0;
    };

    IProcessorPtr_t create_processor(Options& options);
    IInputParserPtr_t create_parser(Options& options);
    ICommandQueuePtr_t create_command_queue();
    ICommandExecutorPtr_t create_command_executor();
} // otus_hw7

