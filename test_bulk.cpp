#include <gtest/gtest.h>
#include <sstream>
#include <list>
#include <tuple>
#ifndef __PRETTY_FUNCTION__
#include "pretty.h"
#endif
#include "bulk_internal.h"

using namespace otus_hw7;

int main(int argc, char **argv)
{
    testing::InitGoogleTest(&argc, argv);
    return RUN_ALL_TESTS();
}

using namespace std::literals::string_literals;

TEST(test_bulk, test_q)
{
    ICommandQueuePtr_t cmd_q = create_command_queue();
    EXPECT_TRUE( cmd_q );
    cmd_q->push(CommandCreator().create_command("Test data"));
    EXPECT_EQ(cmd_q->size(), 1);
    ICommandPtr_t cmd = CommandCreator().create_command("Test data 2");
    cmd_q->push(std::move(cmd));
    EXPECT_EQ(cmd_q->size(), 2);
    cmd_q->pop(cmd);
    EXPECT_EQ(cmd_q->size(), 1);
}

TEST(test_bulk, test_create_q)
{
    ICommandQueuePtr_t cmd_q = create_command_queue();
    EXPECT_TRUE( cmd_q );
}
