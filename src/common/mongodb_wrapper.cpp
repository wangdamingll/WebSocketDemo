#include "mongodb_wrapper.h"
#include "debugtrace.h"
extern "C"{

#ifdef __cplusplus
  #define __STDC_CONSTANT_MACROS
  #ifdef _STDINT_H
   #undef _STDINT_H
  #endif
  # include <stdint.h>
 #endif

}

using namespace std;

void CMongodbConnection::setMongodbAddrAndPort(const string& mongodbAddr, uint16_t mongodbPort) 
{
	mMongodbAddr = mongodbAddr;
	mMongodbPort = mongodbPort;
}

bool CMongodbConnection::connect()
{
	ASSERT(mMongodbConn);
	if (mongo_client(mMongodbConn, mMongodbAddr.c_str(), mMongodbPort) != MONGO_OK) 
	{
		ASSERT(mMongodbConn);
		TRACE(LOG_ERROR, "connect mongodb "<<mMongodbAddr
            <<":"<<mMongodbPort
			<<" mongoerror=>"<<errorInfo(mMongodbConn));
		return false;
	}
	return true;
}

bool CMongodbConnection::mongodbInsert(const string& ns, const bson* data)
{
	ASSERT(mMongodbConn);
	int32_t result = mongo_insert(mMongodbConn, ns.c_str(), data, NULL);
	if (result != MONGO_OK) 
	{
		if (mongo_check_connection(mMongodbConn) != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb "<<mMongodbAddr<<":"<<mMongodbPort<<" is closed, reconnect and retry the request");
			if (mongo_reconnect(mMongodbConn) != MONGO_ERROR)
			{
				result = mongo_insert(mMongodbConn, ns.c_str(), data, NULL);
			} 
			else 
			{
				TRACE(LOG_ERROR, "reconnect mongodb "<<mMongodbAddr<<":"<<mMongodbPort
                    <<" mongoerror=> "<<errorInfo(mMongodbConn));
				TRACE(LOG_ERROR, "Failed to reopen closed mongodb connection to insert query, with ns: "<<ns);
				return false;
			}
		}

		if (result != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb insert error, mongoerror=> "<<errorInfo(mMongodbConn)
				<<", with ns: "<<ns);
			return false;
		}
	}
	return true;
}

bool CMongodbConnection::mongodbDelete(const string& ns, const bson* data)
{
	ASSERT(mMongodbConn);
	int32_t result = mongo_remove(mMongodbConn, ns.c_str(), data, NULL);
	if (result != MONGO_OK) 
	{
		if (mongo_check_connection(mMongodbConn) != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb "<<mMongodbAddr<<":"<<mMongodbPort<<" is closed, reconnect and retry the request");
			if (mongo_reconnect(mMongodbConn) != MONGO_ERROR)
			{
				result = mongo_remove(mMongodbConn, ns.c_str(), data, NULL);
			} 
			else 
			{
				TRACE(LOG_ERROR, "reconnect mongodb "<<mMongodbAddr<<":"<<mMongodbPort
                    <<" mongoerror=> "<<errorInfo(mMongodbConn));
				TRACE(LOG_ERROR, "Failed to reopen closed mongodb connection to delete query, with ns: "<<ns);
				return false;
			}
		}

		if (result != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb delete error, mongoerror=> "<<errorInfo(mMongodbConn)
				<<", with ns: "<<ns);
			return false;
		}
	}
	return true;
}

bool CMongodbConnection::mongodbUpdate(const string& ns, const bson* cond, const bson* op)
{
	ASSERT(mMongodbConn);
	int32_t result = mongo_update(mMongodbConn, ns.c_str(), cond, op, MONGO_UPDATE_UPSERT, NULL);
	if (result != MONGO_OK) 
	{
		if (mongo_check_connection(mMongodbConn) != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb "<<mMongodbAddr<<":"<<mMongodbPort<<" is closed, reconnect and retry the request");
			if (mongo_reconnect(mMongodbConn) != MONGO_ERROR)
			{
				result = mongo_update(mMongodbConn, ns.c_str(), cond, op, MONGO_UPDATE_UPSERT, NULL);
			}
			else 
			{
				TRACE(LOG_ERROR, "reconnect mongodb "<<mMongodbAddr<<":"<<mMongodbPort
                    <<" mongoerror=> "<<errorInfo(mMongodbConn));
				TRACE(LOG_ERROR, "Failed to reopen closed mongodb connection to update query, with ns: "<<ns);
				return false;
			}
		}

		if (result != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb update error, mongoerror=> "<<errorInfo(mMongodbConn)
				<<", with ns: "<<ns);
			return false;
		}
	}
	return true;
}

std::auto_ptr<CCursorStoreResult> CMongodbConnection::mongodbFind(const string& ns, const bson* query, int limit, int skip)
{
	ASSERT(mMongodbConn);
	mongo_cursor *mongoCursor = mongo_find(mMongodbConn, ns.c_str(), query, NULL, limit, skip, 0);
	if (!mongoCursor) 
	{
		if (mongo_check_connection(mMongodbConn) != MONGO_OK) 
		{
			TRACE(LOG_ERROR, "mongodb "<<mMongodbAddr<<":"<<mMongodbPort<<" is closed, reconnect and retry the request");
			if (mongo_reconnect(mMongodbConn) != MONGO_ERROR)
			{
				mongoCursor = mongo_find(mMongodbConn, ns.c_str(), query, NULL, limit, skip, 0);
			} 
			else 
			{
				TRACE(LOG_ERROR, "reconnect mongodb "<<mMongodbAddr<<":"<<mMongodbPort
                    <<" mongoerror=> "<<errorInfo(mMongodbConn));
				TRACE(LOG_ERROR, "Failed to reopen closed mongodb connection to find query, with ns: "<<ns);
				return std::auto_ptr<CCursorStoreResult>();
			}
		}

		if (!mongoCursor) 
		{
			TRACE(LOG_ERROR, "mongodb find error, mongoerror=> "<<errorInfo(mMongodbConn)
				<<", with ns: "<<ns);
			return std::auto_ptr<CCursorStoreResult>();
		}
	}

	std::auto_ptr<CCursorStoreResult> sr = std::auto_ptr<CCursorStoreResult>(new CCursorStoreResult(mongoCursor));
	return sr;
}

bool CMongodbConnection::mongodbCount(const string& db, const string& coll, const bson *query, double &count)
{
  ASSERT(mMongodbConn);
  count = mongo_count(mMongodbConn, db.c_str(), coll.c_str(), query);
  if (count == MONGO_ERROR)
  {
    if (mongo_check_connection(mMongodbConn) != MONGO_OK)
    {
      TRACE(LOG_ERROR, "mongodb "<<mMongodbAddr<<":"<<mMongodbPort<<" is closed, reconnect and retry the request");
      if (mongo_reconnect(mMongodbConn) != MONGO_ERROR)
      {
        count = mongo_count(mMongodbConn, db.c_str(), coll.c_str(), query);
      }
      else
      {
        TRACE(LOG_ERROR, "reconnect mongodb "<<mMongodbAddr<<":"<<mMongodbPort
          <<" mongoerror=> "<<errorInfo(mMongodbConn));
        TRACE(LOG_ERROR, "Failed to reopen closed mongodb connection to count query, with db : "<<db
          <<", coll: "<<coll);
        return false;
      }
    }
    if (count == MONGO_ERROR)
    {
      TRACE(LOG_ERROR, "mongodb count error, mongoerror=> "<<errorInfo(mMongodbConn)
        <<", with db: "<<db
        <<", coll: "<<coll);
      return false;
    }
  }
  return true;
}

std::string CMongodbConnection::mongodbStrerror(mongo_error_t mongoError)
{
	std::string errorString = "";
	switch (mongoError)
	{
	case MONGO_CONN_SUCCESS:
		errorString = "Connection success";
		break;
	case MONGO_CONN_NO_SOCKET:
		errorString = "Could not create a socket";
		break;
	case MONGO_CONN_FAIL:
		errorString = "An error occured while calling connect()";
		break;
	case MONGO_CONN_ADDR_FAIL:
		errorString = "An error occured while calling getaddrinfo()";
		break;
	case MONGO_CONN_NOT_MASTER:
		errorString = "Warning: connected to a non-master node (read-only)";
		break;
	case MONGO_CONN_BAD_SET_NAME:
		errorString = "Given rs name doesn't match this replica set";
		break;
	case MONGO_CONN_NO_PRIMARY:
		errorString = "Can't find primary in replica set. Connection closed";
		break;
	case MONGO_IO_ERROR:
		errorString = "An error occurred while reading or writing on the socket";
		break;
	case MONGO_SOCKET_ERROR:
        errorString = "Other socket error";
        break;
    case MONGO_READ_SIZE_ERROR:
        errorString = "The response is not the expected length";
        break;
    case MONGO_COMMAND_FAILED:
        errorString = "The command returned with 'ok' value of 0";
        break;
    case MONGO_WRITE_ERROR:
        errorString = "Write with given write_concern returned an error";
        break;
    case MONGO_NS_INVALID:
        errorString = "The name for the ns (database or collection) is invalid";
        break;
    case MONGO_BSON_INVALID:
        errorString = "BSON not valid for the specified op";
        break;
    case MONGO_BSON_NOT_FINISHED:
        errorString = "BSON object has not been finished";
        break;
    case MONGO_BSON_TOO_LARGE:
        errorString = "BSON object exceeds max BSON size";
        break;
    case MONGO_WRITE_CONCERN_INVALID:
        errorString = "Supplied write concern object is invalid";
        break;
    }
    return errorString;
}

std::string CMongodbConnection::errorInfo(mongo *mongodbConn)
{
    ASSERT(mongodbConn);
    std::stringstream stream;
    stream<<"err:["<<mongodbConn->err
        <<"]"<<mongodbStrerror(mongodbConn->err)
        <<",errcode:"<<mongodbConn->errcode
        <<",errstr:"<<mongodbConn->errstr
        <<",lasterrcode:"<<mongodbConn->lasterrcode
        <<",lasterrstr:"<<mongodbConn->lasterrstr;
    TRACE(LOG_ERROR, "errorInfo=>err:["<<mongodbConn->err
        <<"]"<<mongodbStrerror(mongodbConn->err)
        <<",errcode:"<<mongodbConn->errcode
        <<",errstr:"<<mongodbConn->errstr
        <<",lasterrcode:"<<mongodbConn->lasterrcode
        <<",lasterrstr:"<<mongodbConn->lasterrstr);
    return stream.str();
}
