#include <gtest/gtest.h>

#include <iostream>
#include <map>
#include <string>
#include <vector>

#include "common.h"

// 测试基本的JSON创建和访问
TEST(JsonTest, BasicOperations) {
  // 创建JSON对象
  json j = {{"name", "张三"},
            {"age", 30},
            {"is_student", false},
            {"scores", {98, 87, 95}},
            {"address", {{"city", "北京"}, {"street", "中关村"}}}};

  // 测试访问属性
  EXPECT_EQ(j["name"], "张三");
  EXPECT_EQ(j["age"], 30);
  EXPECT_EQ(j["is_student"], false);

  // 测试数组访问
  EXPECT_EQ(j["scores"][0], 98);
  EXPECT_EQ(j["scores"][1], 87);
  EXPECT_EQ(j["scores"][2], 95);

  // 测试嵌套对象访问
  EXPECT_EQ(j["address"]["city"], "北京");
  EXPECT_EQ(j["address"]["street"], "中关村");

  // 测试修改值
  j["age"] = 31;
  EXPECT_EQ(j["age"], 31);

  // 测试添加新属性
  j["email"] = "zhangsan@example.com";
  EXPECT_EQ(j["email"], "zhangsan@example.com");
}

// 测试JSON序列化和反序列化
TEST(JsonTest, SerializeDeserialize) {
  json j = {{"name", "李四"}, {"hobbies", {"读书", "游泳", "编程"}}};

  // 序列化为字符串
  std::string serialized = j.dump();

  // 从字符串反序列化
  json parsed = json::parse(serialized);

  EXPECT_EQ(parsed["name"], "李四");
  EXPECT_EQ(parsed["hobbies"][0], "读书");
  EXPECT_EQ(parsed["hobbies"][1], "游泳");
  EXPECT_EQ(parsed["hobbies"][2], "编程");
}

// 测试自定义类型的序列化和反序列化
struct Person {
  std::string name;
  int age;
  std::vector<std::string> hobbies;
};

// 为Person类型定义JSON序列化和反序列化方法
void to_json(json& j, const Person& p) {
  j = json{{"name", p.name}, {"age", p.age}, {"hobbies", p.hobbies}};
}

void from_json(const json& j, Person& p) {
  j.at("name").get_to(p.name);
  j.at("age").get_to(p.age);
  j.at("hobbies").get_to(p.hobbies);
}

TEST(JsonTest, CustomTypeConversion) {
  // 创建Person对象
  Person p{"王五", 25, {"篮球", "音乐"}};

  // 序列化为JSON
  json j = p;

  EXPECT_EQ(j["name"], "王五");
  EXPECT_EQ(j["age"], 25);
  EXPECT_EQ(j["hobbies"][0], "篮球");
  EXPECT_EQ(j["hobbies"][1], "音乐");

  // 反序列化为Person对象
  Person p2 = j.get<Person>();

  EXPECT_EQ(p2.name, "王五");
  EXPECT_EQ(p2.age, 25);
  EXPECT_EQ(p2.hobbies[0], "篮球");
  EXPECT_EQ(p2.hobbies[1], "音乐");
}

// 测试异常处理
TEST(JsonTest, ExceptionHandling) {
  json j = {{"name", "赵六"}};

  // 测试访问不存在的键
  EXPECT_THROW(j.at("age"), json::out_of_range);

  // 测试类型不匹配
  j["mixed"] = "字符串";
  EXPECT_THROW(j["mixed"].get<int>(), json::type_error);

  // 测试解析无效的JSON字符串 - 修改以避免警告
  try {
    json invalid = json::parse("{ invalid json }");
    FAIL() << "应该抛出异常但没有";
  } catch (const json::parse_error& e) {
    // 期望的行为，测试通过
  } catch (...) {
    FAIL() << "抛出了错误类型的异常";
  }
}

int main(int argc, char** argv) {
  ::testing::InitGoogleTest(&argc, argv);
  return RUN_ALL_TESTS();
}
