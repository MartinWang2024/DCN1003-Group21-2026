#pragma once

#include <string>
#include <vector>

struct sqlite3;

namespace dcn_database {

/**
 * @brief 课程实体
 */
struct Course {
	/**
	 * @brief 课程代码
	 */
	std::string code;
	/**
	 * @brief 课程名称
	 */
	std::string title;
	/**
	 * @brief 开课班级或分组
	 */
	std::string section;
	/**
	 * @brief 任课教师
	 */
	std::string instructor;
	/**
	 * @brief 上课日期
	 */
	std::string day;
	/**
	 * @brief 课程时长
	 */
	std::string duration;
	/**
	 * @brief 上课教室
	 */
	std::string classroom;
};

/**
 * @brief 管理员实体
 */
struct Administrator {
	/**
	 * @brief 管理员用户名
	 */
	std::string username;
	/**
	 * @brief 管理员密码
	 */
	std::string password;
};

/**
 * @brief SQLite 数据库连接封装
 * @details 负责连接生命周期管理和最近一次错误信息存储
 */
class Database {
public:
	/**
	 * @brief 默认构造函数
	 */
	Database() = default;
	/**
	 * @brief 析构时自动关闭连接
	 */
	~Database();

	/**
	 * @brief 禁止拷贝构造，避免重复管理同一连接句柄
	 */
	Database(const Database&) = delete;
	/**
	 * @brief 禁止拷贝赋值，避免重复管理同一连接句柄
	 */
	Database& operator=(const Database&) = delete;

	/**
	 * @brief 打开数据库连接
	 * @param db_path 数据库文件路径
	 * @return 打开成功返回 true，否则返回 false 并更新错误信息
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief 关闭数据库连接
	 */
	void close();

	/**
	 * @brief 获取最近一次错误信息
	 * @return 最近一次错误信息字符串引用
	 */
	const std::string& last_error() const;

	/**
	 * @brief 获取底层 SQLite 连接句柄
	 * @return sqlite3 原始句柄，未打开时可能为 nullptr
	 */
	sqlite3* raw_handle() const;

	/**
	 * @brief 设置最近一次错误信息
	 * @param message 错误文本
	 */
	void set_error(const std::string& message) const;

private:
	sqlite3* db_ = nullptr;
	mutable std::string last_error_;
};

/**
 * @brief 课程数据访问仓储
 * @details 提供课程表结构初始化、写入与查询接口
 */
class CourseRepository {
public:
	/**
	 * @brief 默认构造函数
	 */
	CourseRepository();
	/**
	 * @brief 构造并尝试打开指定数据库
	 * @param db_path 数据库文件路径
	 */
	explicit CourseRepository(const std::string& db_path);

	/**
	 * @brief 打开数据库连接
	 * @param db_path 数据库文件路径
	 * @return 打开成功返回 true，否则返回 false
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief 关闭数据库连接
	 */
	void close();
	/**
	 * @brief 初始化课程相关数据表
	 * @return 初始化成功返回 true，否则返回 false
	 */
	bool initialize_schema() const;

	/**
	 * @brief 插入课程记录，若主键或唯一键冲突则覆盖
	 * @param course 待写入课程对象
	 * @return 写入成功返回 true，否则返回 false
	 */
	bool insert_or_replace(const Course& course) const;
	/**
	 * @brief 按课程代码查询课程
	 * @param code 课程代码
	 * @return 匹配课程列表，失败或无结果时可能为空
	 */
	std::vector<Course> search_by_course_code(const std::string& code) const;
	/**
	 * @brief 按教师姓名查询课程
	 * @param instructor 教师姓名
	 * @return 匹配课程列表，失败或无结果时可能为空
	 */
	std::vector<Course> search_by_instructor(const std::string& instructor) const;
	/**
	 * @brief 查询全部课程
	 * @return 全部课程列表，失败时可能为空
	 */
	std::vector<Course> view_all_courses() const;

	/**
	 * @brief 按课程代码和班级删除课程记录
	 * @param code 课程代码
	 * @param section 班级
	 * @return 删除成功返回 true，否则返回 false
	 */
	bool delete_course(const std::string& code, const std::string& section) const;

	/**
	 * @brief 按 (code, section) 更新课程的 title/instructor/day/duration/classroom
	 * @param course 包含新字段值的课程对象，code 和 section 用于定位记录
	 * @return 更新成功返回 true，否则返回 false
	 */
	bool update(const Course& course) const;

	/**
	 * @brief 获取最近一次错误信息
	 * @return 最近一次错误信息字符串引用
	 */
	const std::string& last_error() const;

private:
	/**
	 * @brief 执行查询并将结果集转换为课程对象列表
	 * @param sql SQL 查询语句
	 * @param bindings 预编译参数绑定值列表
	 * @param out_courses 查询结果输出容器
	 * @return 查询并转换成功返回 true，否则返回 false
	 */
	bool prepare_and_collect(const std::string& sql,
							 const std::vector<std::string>& bindings,
							 std::vector<Course>& out_courses) const;

private:
	mutable Database db_;
};

/**
 * @brief 管理员数据访问仓储
 * @details 提供管理员表初始化、写入与登录验证接口
 */
class AdministratorRepository {
public:
	/**
	 * @brief 默认构造函数
	 */
	AdministratorRepository();
	/**
	 * @brief 构造并尝试打开指定数据库
	 * @param db_path 数据库文件路径
	 */
	explicit AdministratorRepository(const std::string& db_path);

	/**
	 * @brief 打开数据库连接
	 * @param db_path 数据库文件路径
	 * @return 打开成功返回 true，否则返回 false
	 */
	bool open(const std::string& db_path);
	/**
	 * @brief 关闭数据库连接
	 */
	void close();
	/**
	 * @brief 初始化管理员相关数据表
	 * @return 初始化成功返回 true，否则返回 false
	 */
	bool initialize_schema() const;

	/**
	 * @brief 插入管理员记录，若冲突则覆盖
	 * @param admin 待写入管理员对象
	 * @return 写入成功返回 true，否则返回 false
	 */
	bool insert_or_replace(const Administrator& admin) const;
	/**
	 * @brief 校验管理员登录凭据
	 * @param username 用户名
	 * @param password 密码
	 * @return 凭据匹配返回 true，否则返回 false
	 */
	bool verify_login(const std::string& username, const std::string& password) const;

	/**
	 * @brief 获取最近一次错误信息
	 * @return 最近一次错误信息字符串引用
	 */
	const std::string& last_error() const;

private:
	mutable Database db_;
};

}  // namespace dcn_database
