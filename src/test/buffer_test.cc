#include "buffer.h"

#include <gtest/gtest.h>

#include <csignal>
#include <iostream>

#include "raft.h"
#include "test_suite.h"
#include "timer.h"
#include "util.h"

//--------------------------------
// EXPECT_TRUE  true
// EXPECT_FALSE false
//
// ASSERT_TRUE  true
// ASSERT_FALSE false
//
// EXPECT_EQ  ==
// EXPECT_NE  !=
// EXPECT_NE  <
// EXPECT_LE  <=
// EXPECT_GT  >
// EXPECT_GE  >=
//
// ASSERT_EQ  ==
// ASSERT_NE  !=
// ASSERT_LT  <
// ASSERT_LE  <=
// ASSERT_GT  >
// ASSERT_GE  >=
//--------------------------------

void PrintBuf(vraft::Buffer &buf) {
  std::cout << "init_bytes:" << buf.init_bytes() << std::endl;
  std::cout << "max_waste_bytes:" << buf.max_waste_bytes() << std::endl;
  std::cout << "buf().size:" << buf.buf().size() << std::endl;
  std::cout << "buf().capacity:" << buf.buf().capacity() << std::endl;
  std::cout << "read_index:" << buf.read_index() << std::endl;
  std::cout << "write_index:" << buf.write_index() << std::endl;
  std::cout << "ReadableBytes:" << buf.ReadableBytes() << std::endl;
  std::cout << "WritableBytes:" << buf.WritableBytes() << std::endl;
  std::cout << "WastefulBytes:" << buf.WastefulBytes() << std::endl;
  std::cout << std::endl;
}

void AssertInit(vraft::Buffer &buf) {
  ASSERT_EQ(buf.init_bytes(), BUF_INIT_BYTES);
  ASSERT_EQ(buf.max_waste_bytes(), BUF_MAX_WASTE_BYTES);
  ASSERT_EQ(buf.read_index(), 0);
  ASSERT_EQ(buf.write_index(), 0);
  ASSERT_EQ(buf.ReadableBytes(), 0);
  ASSERT_GE(buf.WritableBytes(), BUF_INIT_BYTES);
  ASSERT_EQ(buf.WastefulBytes(), 0);
}

TEST(Buffer, ctor) {
  vraft::Buffer buf;
  PrintBuf(buf);
  AssertInit(buf);
}

TEST(Buffer, PeekInt8) {
  vraft::Buffer buf;

  int8_t x = 6;
  buf.Append(reinterpret_cast<const char *>(&x), sizeof(int8_t));
  PrintBuf(buf);

  ASSERT_TRUE(static_cast<int32_t>(buf.buf().capacity()) >=
              buf.ReadableBytes());
  ASSERT_EQ(buf.read_index(), 0);
  ASSERT_EQ(buf.write_index(), static_cast<int32_t>(sizeof(int8_t)));
  ASSERT_EQ(buf.ReadableBytes(), static_cast<int32_t>(sizeof(int8_t)));
  ASSERT_EQ(buf.WritableBytes(),
            static_cast<int32_t>(BUF_INIT_BYTES - sizeof(int8_t)));
  ASSERT_EQ(buf.WastefulBytes(), 0);

  int8_t xx = buf.PeekInt8();
  ASSERT_EQ(x, xx);

  buf.RetrieveInt8();
  PrintBuf(buf);
  AssertInit(buf);
}

TEST(Buffer, PeekInt8_2) {
  vraft::Buffer buf;

  int32_t len = BUF_INIT_BYTES;
  char *ptr = (char *)malloc(len);
  buf.Append(ptr, len);
  free(ptr);
  PrintBuf(buf);
  ASSERT_EQ(buf.WritableBytes(), static_cast<int32_t>(0));

  int8_t x = 6;
  buf.Append(reinterpret_cast<const char *>(&x), sizeof(int8_t));
  PrintBuf(buf);

  ASSERT_TRUE(static_cast<int32_t>(buf.buf().capacity()) >=
              buf.ReadableBytes());
  ASSERT_EQ(buf.read_index(), 0);
  ASSERT_EQ(buf.write_index(),
            BUF_INIT_BYTES + static_cast<int32_t>(sizeof(int8_t)));
  ASSERT_EQ(buf.ReadableBytes(),
            BUF_INIT_BYTES + static_cast<int32_t>(sizeof(int8_t)));
  ASSERT_EQ(buf.WritableBytes(), 0);
  ASSERT_EQ(buf.WastefulBytes(), 0);

  buf.Retrieve(len);
  PrintBuf(buf);
  ASSERT_TRUE(static_cast<int32_t>(buf.buf().capacity()) >=
              buf.ReadableBytes());
  ASSERT_EQ(buf.read_index(), 0);
  ASSERT_EQ(buf.write_index(), 1);
  ASSERT_EQ(buf.ReadableBytes(), 1);
  ASSERT_EQ(buf.WritableBytes(), 65536);
  ASSERT_EQ(buf.WastefulBytes(), 0);

  int8_t xx = buf.PeekInt8();
  ASSERT_EQ(x, xx);

  buf.RetrieveInt8();
  PrintBuf(buf);
  AssertInit(buf);
}

TEST(Buffer, Retrieve) {
  vraft::Buffer buf;
  PrintBuf(buf);
  AssertInit(buf);

  int8_t i8 = 8;
  buf.Append(reinterpret_cast<const char *>(&i8), sizeof(i8));

  int16_t i16 = 16;
  buf.Append(reinterpret_cast<const char *>(&i16), sizeof(i16));

  int32_t i32 = 32;
  buf.Append(reinterpret_cast<const char *>(&i32), sizeof(i32));

  int64_t i64 = 64;
  buf.Append(reinterpret_cast<const char *>(&i64), sizeof(i64));

  int8_t i8_ = buf.PeekInt8();
  ASSERT_EQ(i8, i8_);
  buf.RetrieveInt8();

  int16_t i16_ = buf.PeekInt16();
  ASSERT_EQ(i16, i16_);
  buf.RetrieveInt16();

  int32_t i32_ = buf.PeekInt32();
  ASSERT_EQ(i32, i32_);
  buf.RetrieveInt32();

  int64_t i64_ = buf.PeekInt64();
  ASSERT_EQ(i64, i64_);
  buf.RetrieveInt64();

  AssertInit(buf);
}

TEST(Buffer, RetrieveAll) {
  vraft::Buffer buf;
  PrintBuf(buf);
  AssertInit(buf);

  for (int i = 0; i < 10; ++i) {
    int32_t len = BUF_INIT_BYTES;
    char *ptr = (char *)malloc(len);
    buf.Append(ptr, len);
    free(ptr);
  }

  PrintBuf(buf);
  buf.RetrieveAll();
  PrintBuf(buf);
  AssertInit(buf);
}

int main(int argc, char **argv) {
  vraft::CodingInit();
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}