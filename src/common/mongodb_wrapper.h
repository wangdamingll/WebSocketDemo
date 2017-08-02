/*
 * mongodb_wrapper.h
 * 
 *	Created on:	2014-6-25
 * 		Author:	WengJun
 *
 *	
 */

#ifndef MONGODB_WRAPPER_H
#define MONGODB_WRAPPER_H

extern "C"{

#ifdef __cplusplus
  #define __STDC_CONSTANT_MACROS
  #ifdef _STDINT_H
   #undef _STDINT_H
  #endif
  # include <stdint.h>
 #endif

}
#include <stdint.h>
#include <string>
#include "debugtrace.h"
#include "mongodb/bson.h"
#include "mongodb/mongo.h"

using namespace std;

class CCursorResultBase
{
protected:
	mongo_cursor *mMongoCursor;

public:
	CCursorResultBase(mongo_cursor* mongoCursor) : mMongoCursor(mongoCursor) {}
	~CCursorResultBase()
	{
		if (mMongoCursor) 
		{
			mongo_cursor_destroy(mMongoCursor);
		}
	}

	void getField(const std::string& fieldIndex, double& value)
	{
		bson_iterator bi[1];
		if (bson_find(bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO) 
		{
			value = bson_iterator_double(bi);
		} 
		else 
		{
			value = 0.0;
		}
	}
	void getField(const std::string& fieldIndex, int32_t& value)
	{
		bson_iterator bi[1];
		if (bson_find(bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO) 
		{
			value = bson_iterator_int(bi);
		} 
		else 
		{
			value = 0;
		}
	}
	void getField(const std::string& fieldIndex, int64_t& value)
	{
		bson_iterator bi[1];
		if (bson_find(bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO) 
		{
			value = bson_iterator_long(bi);
		} 
		else 
		{
			value = 0;
		}
	}
	void getField(const std::string& fieldIndex, bool& value)
	{
		bson_iterator bi[1];
		if (bson_find(bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO) 
		{
			value = bson_iterator_bool(bi) != 0;
		} 
		else 
		{
			value = false;
		}
	}
	void getField(const std::string& fieldIndex, string& value)
	{
		bson_iterator bi[1];
		if (bson_find(bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO) 
		{
			value = bson_iterator_string(bi);
		} 
		else 
		{
			value = "";
		}
	}
    bool getField(const std::string& fieldIndex, bson_iterator &bi)
    {
        if (bson_find(&bi, &(mMongoCursor->current), fieldIndex.c_str()) != BSON_EOO)
        {
            return true;
        }
        else
        {
            return false;
        }
    }
};

class CCursorStoreResult : public CCursorResultBase
{
public:
	CCursorStoreResult(mongo_cursor* mongoCursor) : CCursorResultBase(mongoCursor)
	{}

public:
	int32_t getNumRows()
	{
		return mMongoCursor->seen;
	}

	bool fetchRow()
	{
		return mongo_cursor_next(mMongoCursor) == MONGO_OK;
	}
};

class CMongodbConnection
{
public:
	CMongodbConnection() : mMongodbConn(NULL) 
	{
		mMongodbConn = new mongo;
		mMongodbConn->primary = NULL;
		mMongodbConn->replica_set = NULL;
		mMongodbConn->write_concern = NULL;
	}

	~CMongodbConnection()
	{
		if (mMongodbConn) 
		{
			mongo_destroy(mMongodbConn);
		}
	}

	//设置mongodb地址
	void setMongodbAddrAndPort(const string& mongodbAddr, uint16_t mongodbPort);

	//连接mongodb
	bool connect();

	//insert操作
	bool mongodbInsert(const string& ns, const bson* data);
	//delete操作
	bool mongodbDelete(const string& ns, const bson* data);
	//update操作
	bool mongodbUpdate(const string& ns, const bson* cond, const bson* op);
	//find操作
	std::auto_ptr<CCursorStoreResult> mongodbFind(const string& ns, const bson* query, int limit = INT_MAX, int skip = 0);
  //查找总条数
  bool mongodbCount(const string& db, const string& coll, const bson *query, double &count);

private:
	//mongodb错误号转化为字符串
	std::string mongodbStrerror(mongo_error_t mongoError);
    std::string errorInfo(mongo *mongodbConn);

private:
	//mongodb地址
	string mMongodbAddr;
	uint16_t mMongodbPort;
	//mongodb对象
	mongo *mMongodbConn;
};

#endif
