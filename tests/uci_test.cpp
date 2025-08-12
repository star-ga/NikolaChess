#include <gtest/gtest.h>
#include "board.h"

TEST(UCITest, FenRoundTrip) {
    std::string start = "rnbqkbnr/pppppppp/8/8/8/8/PPPPPPPP/RNBQKBNR w KQkq - 0 1";
    nikola::Board b = nikola::parseFEN(start);
    std::string out = nikola::boardToFEN(b);
    EXPECT_EQ(out.substr(0, start.find(' ')), start.substr(0, start.find(' ')));
}
