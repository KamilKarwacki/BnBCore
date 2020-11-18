#include<gtest/gtest.h>
#include "../include/MPI_Message_Encoder.h"
#include <tuple>
#include <sstream>

TEST(BasicTypeTest, DemonstratedGTestMacros)
{
	std::stringstream ss("");

	std::tuple<int, float, double> message {10, 20.f, 30.0};

	MPI_Message_Encoder<std::tuple<int, float, double>> encoder;
	encoder.Encode_Solution(ss, message);	

	// other processor decoding now
	std::tuple<int,float,double> receivedMessage;
	encoder.Decode_Solution(ss, receivedMessage);

	EXPECT_EQ(std::get<0>(receivedMessage), 10) <<  "integer encoding didnt work";
	EXPECT_EQ(std::get<1>(receivedMessage), 20.f) <<"float encoding didnt work";
	EXPECT_EQ(std::get<2>(receivedMessage), 30.0) <<"double encoding didnt work";
}	


TEST(VectorTest, DemonstratedGTestMacros)
{
	std::stringstream ss("");

	std::tuple<std::vector<int>> message {{1,2,3}};
	MPI_Message_Encoder<std::tuple<std::vector<int>>> encoder;
	encoder.Encode_Solution(ss, message);	

	// other processor decoding now
	std::tuple<std::vector<int>> receivedMessage;
	encoder.Decode_Solution(ss, receivedMessage);

	EXPECT_EQ(std::get<0>(receivedMessage)[0], 1) <<  "integer encoding didnt work";
	EXPECT_EQ(std::get<0>(receivedMessage)[1], 2) << "float encoding didnt work";
	EXPECT_EQ(std::get<0>(receivedMessage)[2], 3) << "double encoding didnt work";
}	

int main(int argc, char* argv[])
{
	testing::InitGoogleTest(&argc, argv);
	return RUN_ALL_TESTS();
}
