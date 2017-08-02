
#ifndef _CONFIGURE_H_
#define _CONFIGURE_H_

//#include "include.h"
#include <string>

#define ASCIILINESZ         (1024)
#define INI_INVALID_KEY     ((char*)-1)

typedef enum _line_status_ 
{
	LINE_UNPROCESSED,
	LINE_ERROR,
	LINE_EMPTY,
	LINE_COMMENT,
	LINE_SECTION,
	LINE_VALUE
} line_status;

class CConfigure
{
public:
	CConfigure();
	virtual ~CConfigure();

	void set_conf_file_name(const char* apFileName);
	const std::string get_conf_file_name(void){ return _file_name; }
	bool load();
	std::string GetStrParam(const std::string& astrKey,const std::string& astrSection,const std::string& astrCfgFile = "");
	int GetIntParam(const std::string& astrKey,const std::string& astrSection,const std::string& astrCfgFile = "");
protected:
	line_status iniparser_line(
		char * input_line,
		char * section,
		char * key,
		char * value);
	char * strstrip(char * s);
	char * strlwc(const char * s);
	virtual bool parse_value(const char* key, const char* value){return true;}
protected:
    std::string _file_name;
};


#endif //_CONFIGURE_H_













