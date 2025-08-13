#include <gtest/gtest.h>
#include "distributed.h"

TEST(DistributedSearchTest, LocalSearchCompletes) {
    // The distributed_search function should return 0 regardless of whether
    // MPI support is enabled.  In this test environment MPI is typically
    // disabled, so the function exercises the thread-based fallback path.
    EXPECT_EQ(0, nikola::distributed_search());
}

