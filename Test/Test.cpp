// Test.cpp : Defines the entry point for the console application.
//

/*

说明：
	该工程的 Runtime Library 属性必须与 LevelDB.lib 的 Runtime Library 属性保持一致
	比如都为 Multi-threaded Debug(/MTd)

*/

#include "stdafx.h"
#include <assert.h>
#include <iostream>
#include <string>

#include "leveldb/db.h"

#pragma comment(lib,"../Lib/LevelDB.lib")


int _tmain(int argc, _TCHAR* argv[])
{
	leveldb::DB* db;
	leveldb::Options options;
	options.create_if_missing = true;
	leveldb::Status status = leveldb::DB::Open(options, "testdb", &db);
	assert(status.ok());

	std::string key = "key1";
	std::string value = "value1";
	status = db->Put(leveldb::WriteOptions(), key, value);
	assert(status.ok());
	status = db->Get(leveldb::ReadOptions(), key, &value);
	assert(status.ok());
	std::cout << key << "=>" << value << std::endl;

	std::string key2 = "key2";
	status = db->Put(leveldb::WriteOptions(), key2, value);
	assert(status.ok());
	status = db->Delete(leveldb::WriteOptions(), key);
	assert(status.ok());
	status = db->Get(leveldb::ReadOptions(), key2, &value);
	assert(status.ok());
	std::cout << key2 << "=>" << value << std::endl;

	status = db->Get(leveldb::ReadOptions(), key, &value);
	if (!status.ok())
	{
		std::cerr << key << "=>" << status.ToString() << std::endl;
	}
	else
	{
		std::cout << key << "=>" << value << std::endl;
	}
	delete db;

	getchar();
	return 0;
}
