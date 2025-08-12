#include <gtest/gtest.h>
#include "board.h"

TEST(EvaluationTest, StartPositionBalanced) {
    nikola::Board b = nikola::initBoard();
    int score = nikola::evaluateBoardCPU(b);
    EXPECT_NEAR(score, 0, 50);
}
