// 编写一个测试用例，测试protobuf的序列化和反序列化功能
// 将如下结构体序列化，反序列化
// struct Person {
//    std::string name;
//    int age;
//    std::string email;
//};

#include <fstream>
#include <iostream>
#include <string>

#include "gtest/gtest.h"
#include "person.pb.h"

// 测试protobuf的序列化和反序列化功能
TEST(ProtobufTest, SerializeDeserialize) {
  // 初始化protobuf库
  GOOGLE_PROTOBUF_VERIFY_VERSION;

  // 创建一个Person对象
  test::Person person;
  person.set_name("张三");
  person.set_age(30);
  person.set_email("zhangsan@example.com");

  // 序列化到字符串
  std::string serialized_str;
  ASSERT_TRUE(person.SerializeToString(&serialized_str));

  std::cout << "序列化后的数据大小: " << serialized_str.size() << " 字节"
            << std::endl;

  // 反序列化
  test::Person deserialized_person;
  ASSERT_TRUE(deserialized_person.ParseFromString(serialized_str));

  // 验证反序列化后的数据是否正确
  EXPECT_EQ(deserialized_person.name(), "张三");
  EXPECT_EQ(deserialized_person.age(), 30);
  EXPECT_EQ(deserialized_person.email(), "zhangsan@example.com");

  // 序列化到文件
  std::string filename = "person.data";
  std::ofstream ofs(filename, std::ios::binary);
  ASSERT_TRUE(person.SerializeToOstream(&ofs));
  ofs.close();

  // 从文件反序列化
  test::Person file_person;
  std::ifstream ifs(filename, std::ios::binary);
  ASSERT_TRUE(file_person.ParseFromIstream(&ifs));
  ifs.close();

  // 验证从文件反序列化的数据是否正确
  EXPECT_EQ(file_person.name(), "张三");
  EXPECT_EQ(file_person.age(), 30);
  EXPECT_EQ(file_person.email(), "zhangsan@example.com");

  // 清理
  std::remove(filename.c_str());

  // 可选：关闭protobuf库
  google::protobuf::ShutdownProtobufLibrary();
}

int main(int argc, char **argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
