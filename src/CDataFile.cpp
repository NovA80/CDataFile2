//
// CDataFile Class Implementation
//
// The purpose of this class is to provide a simple, full featured means to
// store persistent data to a text file.  It uses a simple key/value paradigm
// to achieve this.  The class can read/write to standard Windows .ini files,
// and yet does not rely on any windows specific calls.  It should work as
// well in a linux environment (with some minor adjustments) as it does in
// a Windows one.
//
// Written July, 2002 by Gary McNickle <gary#sunstorm.net>
// If you use this class in your application, credit would be appreciated.
//
// Updated January, 2017 by Andrey Novikov <AndrewNovikov@yandex.ru>
//

//
// CDataFile
// The purpose of this class is to provide the means to easily store key/value
// pairs in a config file, seperated by independant sections. Sections may not
// have duplicate keys, although two or more sections can have the same key.
// Simple support for comments is included. Each key, and each section may have
// it's own multiline comment.
//
// An example might look like this;
//
// [UserSettings]
// Name=Joe User
// Date of Birth=12/25/01
//
// ;
// ; Settings unique to this server
// ;
// [ServerSettings]
// Port=1200
// IP_Address=127.0.0.1
// MachineName=ADMIN
//

#include <vector>
#include <string>
#include <ctype.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h> // memset
#include <fstream>
#include <sstream>

#if defined(WIN32)
	#include <windows.h>
#endif

#include "CDataFile.h"
using namespace cdf;

// Compatibility Defines ////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////
#ifdef WIN32
	#define snprintf  _snprintf
	#define vsnprintf _vsnprintf
#endif


// CDataFile
// Our default contstructor.  If it can load the file, it will do so and populate
// the section list with the values from the file.
cdf::CDataFile::CDataFile(const t_Str &szFileName)
{
	m_szFileName = szFileName;
	m_Flags = (AUTOCREATE_SECTIONS | AUTOCREATE_KEYS);
	m_Sections.push_back( *(new t_Section) );

	Load(m_szFileName);
	m_bDirty = false;
}

cdf::CDataFile::CDataFile()
{
	Clear();
	m_Flags = (AUTOCREATE_SECTIONS | AUTOCREATE_KEYS);
	m_Sections.push_back( *(new t_Section) );
}

// ~CDataFile
// Saves the file if any values have changed since the last save.
cdf::CDataFile::~CDataFile()
{
	if ( m_bDirty )
		Save();
}

// Clear
// Resets the member variables to their defaults
void cdf::CDataFile::Clear()
{
	m_bDirty = false;
	m_szFileName = t_Str("");
	m_Sections.clear();
}

// SetDirty
// Sets the 'dirty' flag to mark the data as changed or not
void cdf::CDataFile::SetDirty(bool dirty)
{
	m_bDirty = dirty;
}

// IsDirty
// Obtains the 'dirty' flag showing the data is changed or not
bool cdf::CDataFile::IsDirty() const
{
	return m_bDirty;
}


// SetFileName
// Set's the m_szFileName member variable. For use when creating the CDataFile
// object by hand (-vs- loading it from a file
void cdf::CDataFile::SetFileName(const t_Str &szFileName)
{
	if (m_szFileName.size() != 0 && CompareNoCase(szFileName, m_szFileName) != 0)
	{
		m_bDirty = true;

		Report(E_WARN, "[CDataFile::SetFileName] The filename has changed from <%s> to <%s>.",
			m_szFileName.c_str(), szFileName.c_str());
	}

	m_szFileName = szFileName;
}

// Load
// Attempts to load in the text file. If successful it will populate the
// Section list with the key/value pairs found in the file. Note that comments
// are saved so that they can be rewritten to the file later.
bool cdf::CDataFile::Load(const t_Str& szFileName)
{
	// We dont want to create a new file here.  If it doesn't exist, just
	// return false and report the failure.
	std::ifstream File(szFileName.c_str());
	if ( ! File.is_open() )
	{
		Report(E_INFO, "[CDataFile::Load] Unable to open file. Does it exist?");
		return false;
	}

	bool bDone = false;
	bool bAutoKey = (m_Flags & AUTOCREATE_KEYS) == AUTOCREATE_KEYS;
	bool bAutoSec = (m_Flags & AUTOCREATE_SECTIONS) == AUTOCREATE_SECTIONS;

	t_Str szLine;
	t_Str szComment;
	char buffer[MAX_BUFFER_LEN];
	t_Section* pSection = GetSection("");

	// These need to be set, we'll restore the original values later.
	m_Flags |= AUTOCREATE_KEYS;
	m_Flags |= AUTOCREATE_SECTIONS;

	while ( !bDone )
	{
		memset(buffer, 0, MAX_BUFFER_LEN);
		File.getline(buffer, MAX_BUFFER_LEN);

		szLine = buffer;
		Trim(szLine);

		bDone = ( File.eof() || File.bad() || File.fail() );

		if ( szLine.find_first_of(CommentIndicators) == 0 )
		{
			szComment += "\n";
			szComment += szLine;
		}
		else
		if ( szLine.find_first_of('[') == 0 ) // new section
		{
			szLine.erase( 0, 1 );
			szLine.erase( szLine.find_last_of(']'), 1 );

			CreateSection(szLine, szComment);
			pSection = GetSection(szLine);
			szComment = t_Str("");
		}
		else
		if ( szLine.size() > 0 ) // we have a key, add this key/value pair
		{
			t_Str szKey = GetNextWord(szLine);
			t_Str szValue = szLine;

			if ( szKey.size() > 0 )
			{
				SetValue(szKey, szValue, szComment, pSection->szName);
				szComment = t_Str("");
			}
		}
	}

	// Restore the original flag values.
	if ( !bAutoKey )
		m_Flags &= ~AUTOCREATE_KEYS;

	if ( !bAutoSec )
		m_Flags &= ~AUTOCREATE_SECTIONS;

	File.close();

	return true;
}


// Save
// Attempts to save the Section list and keys to the file. Note that if Load
// was never called (the CDataFile object was created manually), then you
// must set the m_szFileName variable before calling save.
bool cdf::CDataFile::Save()
{
	if ( KeyCount() == 0 && SectionCount() == 0 )
	{
		// no point in saving
		Report(E_INFO, "[CDataFile::Save] Nothing to save.");
		return false;
	}

	if ( m_szFileName.size() == 0 )
	{
		Report(E_ERROR, "[CDataFile::Save] No filename has been set.");
		return false;
	}

	std::ofstream File(m_szFileName.c_str(), std::ios::out|std::ios::trunc);
	if ( ! File.is_open() )
	{
		Report(E_ERROR, "[CDataFile::Save] Unable to save file.");
		return false;
	}

	SectionItor s_pos;
	KeyItor k_pos;
	t_Section Section;
	t_Key Key;

	for (s_pos = m_Sections.begin(); s_pos != m_Sections.end(); s_pos++)
	{
		Section = (*s_pos);
		bool bWroteComment = false;

		if ( Section.szComment.size() > 0 )
		{
			bWroteComment = true;
			WriteLn(File, "\n%s", CommentStr(Section.szComment).c_str());
		}

		if ( Section.szName.size() > 0 )
		{
			WriteLn(File, "%s[%s]",
					bWroteComment ? "" : "\n",
					Section.szName.c_str());
		}

		for (k_pos = Section.Keys.begin(); k_pos != Section.Keys.end(); k_pos++)
		{
			Key = (*k_pos);

			if ( Key.szKey.size() > 0 )
			{
				WriteLn(File, "%s%s%s%s%c%s",
					Key.szComment.size() > 0 ? "\n" : "",
					CommentStr(Key.szComment).c_str(),
					Key.szComment.size() > 0 ? "\n" : "",
					Key.szKey.c_str(),
					EqualIndicators[0],
					Key.szValue.c_str());
			}
		}
	}

	m_bDirty = false;

	File.flush();
	File.close();

	return true;
}

// SetKeyComment
// Set the comment of a given key. Returns true if the key is not found.
bool cdf::CDataFile::SetKeyComment(const t_Str &szKey, const t_Str &szComment, const t_Str &szSection)
{
	KeyItor k_pos;
	t_Section* pSection;

	if ( (pSection = GetSection(szSection)) == NULL )
		return false;

	for (k_pos = pSection->Keys.begin(); k_pos != pSection->Keys.end(); k_pos++)
	{
		if ( CompareNoCase( (*k_pos).szKey, szKey ) == 0 )
		{
			(*k_pos).szComment = szComment;
			m_bDirty = true;
			return true;
		}
	}

	return false;
}

// SetSectionComment
// Set the comment for a given section. Returns false if the section
// was not found.
bool cdf::CDataFile::SetSectionComment(const t_Str &szSection, const t_Str &szComment)
{
	SectionItor s_pos;

	for (s_pos = m_Sections.begin(); s_pos != m_Sections.end(); s_pos++)
	{
		if ( CompareNoCase( (*s_pos).szName, szSection ) == 0 )
		{
			(*s_pos).szComment = szComment;
			m_bDirty = true;
			return true;
		}
	}

	return false;
}


// SetValue
// Given a key, a value and a section, this function will attempt to locate the
// Key within the given section, and if it finds it, change the keys value to
// the new value. If it does not locate the key, it will create a new key with
// the proper value and place it in the section requested.
bool cdf::CDataFile::SetValue(const t_Str &szKey, const t_Str &szValue, const t_Str &szComment, const t_Str &szSection)
{
	t_Key* pKey = GetKey(szKey, szSection);
	t_Section* pSection = GetSection(szSection);

	if (pSection == NULL)
	{
		if ( !(m_Flags & AUTOCREATE_SECTIONS) || !CreateSection(szSection,""))
			return false;

		pSection = GetSection(szSection);
	}

	// Sanity check...
	if ( pSection == NULL )
		return false;

	// if the key does not exist in that section then add the new key.
	if ( pKey == NULL && (m_Flags & AUTOCREATE_KEYS))
	{
		pKey = new t_Key;

		pKey->szKey = szKey;
		pKey->szValue = szValue;
		pKey->szComment = szComment;

		m_bDirty = true;

		pSection->Keys.push_back(*pKey);

		return true;
	}

	if ( pKey != NULL )
	{
		pKey->szValue = szValue;
		pKey->szComment = szComment;

		m_bDirty = true;

		return true;
	}

	return false;
}

// SetFloat
// Passes the given float to SetValue as a string
bool cdf::CDataFile::SetFloat(const t_Str &szKey, float fValue, const t_Str &szComment, const t_Str &szSection)
{
	char szStr[64];

	snprintf(szStr, 64, "%g", fValue);

	return SetValue(szKey, szStr, szComment, szSection);
}

// SetInt
// Passes the given int to SetValue as a string
bool cdf::CDataFile::SetInt(const t_Str &szKey, int nValue, t_Str szComment, t_Str szSection)
{
	char szStr[64];

	snprintf(szStr, 64, "%d", nValue);

	return SetValue(szKey, szStr, szComment, szSection);

}

// SetBool
// Passes the given bool to SetValue as a string
bool cdf::CDataFile::SetBool(const t_Str &szKey, bool bValue, t_Str szComment, t_Str szSection)
{
	t_Str szValue = bValue ?  "True" : "False";

	return SetValue(szKey, szValue, szComment, szSection);
}

// GetValue
// Obtains the key value as a t_Str object. Returns false
// if the key could not be found.
bool cdf::CDataFile::GetValue(const t_Str &szKey, const t_Str &szSection, t_Str& ret)
{
	t_Key* pKey = GetKey(szKey, szSection);
	if( ! pKey )
		return false;

	ret = pKey->szValue;
	return true;
}

// GetString
// Obtains the key value as a t_Str object. Returns false
// if the key could not be found.
bool cdf::CDataFile::GetString(const t_Str &szKey, const t_Str &szSection, t_Str &ret)
{
	return GetValue(szKey, szSection, ret);
}

// GetFloat
// Obtains the key value as a float type. Returns false if the key is
// not found.
bool cdf::CDataFile::GetFloat(const t_Str &szKey, const t_Str &szSection, float &ret)
{
	t_Str szValue;
	if( ! GetValue(szKey, szSection, szValue) )
		return false;

	float x;
	if( (std::istringstream(szValue) >> x).fail() )
		return false;

	ret = x;
	return true;
}

// GetInt
// Obtains the key value as an integer type. Returns false if the key is
// not found.
bool cdf::CDataFile::GetInt(const t_Str &szKey, const t_Str &szSection, int &ret)
{
	t_Str szValue;
	if( ! GetValue(szKey, szSection, szValue) )
		return false;

	int n;
	if( (std::istringstream(szValue) >> n).fail() )
		return false;

	ret = n;
	return true;
}

// GetBool
// Obtains the key value as a bool type. Returns false if the key is
// not found.
bool cdf::CDataFile::GetBool(const t_Str &szKey, const t_Str &szSection, bool &ret)
{
	t_Str szValue;
	if( ! GetValue(szKey, szSection, szValue) )
		return false;

	ret = false;
	if ( szValue.find("1") == 0
		|| CompareNoCase(szValue, "true") == 0
		|| CompareNoCase(szValue, "yes") == 0 ) {
		ret = true;
	}

	return true;
}

// DeleteSection
// Delete a specific section. Returns false if the section cannot be
// found or true when sucessfully deleted.
bool cdf::CDataFile::DeleteSection(const t_Str &szSection)
{
	SectionItor s_pos;

	for (s_pos = m_Sections.begin(); s_pos != m_Sections.end(); s_pos++)
	{
		if ( CompareNoCase( (*s_pos).szName, szSection ) == 0 )
		{
			m_Sections.erase(s_pos);
			return true;
		}
	}

	return false;
}

// DeleteKey
// Delete a specific key in a specific section. Returns false if the key
// cannot be found or true when sucessfully deleted.
bool cdf::CDataFile::DeleteKey(const t_Str &szKey, const t_Str &szFromSection)
{
	KeyItor k_pos;
	t_Section* pSection;

	if ( (pSection = GetSection(szFromSection)) == NULL )
		return false;

	for (k_pos = pSection->Keys.begin(); k_pos != pSection->Keys.end(); k_pos++)
	{
		if ( CompareNoCase( (*k_pos).szKey, szKey ) == 0 )
		{
			pSection->Keys.erase(k_pos);
			return true;
		}
	}

	return false;
}

// CreateKey
// Given a key, a value and a section, this function will attempt to locate the
// Key within the given section, and if it finds it, change the keys value to
// the new value. If it does not locate the key, it will create a new key with
// the proper value and place it in the section requested.
bool cdf::CDataFile::CreateKey(const t_Str &szKey, const t_Str &szValue, const t_Str &szComment, const t_Str &szSection)
{
	bool bAutoKey = (m_Flags & AUTOCREATE_KEYS) == AUTOCREATE_KEYS;
	bool bReturn  = false;

	m_Flags |= AUTOCREATE_KEYS;

	bReturn = SetValue(szKey, szValue, szComment, szSection);

	if ( !bAutoKey )
		m_Flags &= ~AUTOCREATE_KEYS;

	return bReturn;
}


// CreateSection
// Given a section name, this function first checks to see if the given section
// allready exists in the list or not, if not, it creates the new section and
// assigns it the comment given in szComment.  The function returns true if
// sucessfully created, or false otherwise.
bool cdf::CDataFile::CreateSection(const t_Str &szSection, const t_Str &szComment)
{
	t_Section* pSection = GetSection(szSection);

	if ( pSection )
	{
		Report(E_INFO, "[CDataFile::CreateSection] Section <%s> allready exists. Aborting.", szSection.c_str());
		return false;
	}

	pSection = new t_Section;

	pSection->szName = szSection;
	pSection->szComment = szComment;
	m_Sections.push_back(*pSection);
	m_bDirty = true;

	return true;
}

// CreateSection
// Given a section name, this function first checks to see if the given section
// allready exists in the list or not, if not, it creates the new section and
// assigns it the comment given in szComment.  The function returns true if
// sucessfully created, or false otherwise. This version accpets a KeyList
// and sets up the newly created Section with the keys in the list.
bool cdf::CDataFile::CreateSection(const t_Str &szSection, const t_Str &szComment, KeyList Keys)
{
	if ( !CreateSection(szSection, szComment) )
		return false;

	t_Section* pSection = GetSection(szSection);

	if ( !pSection )
		return false;

	KeyItor k_pos;

	pSection->szName = szSection;
	for (k_pos = Keys.begin(); k_pos != Keys.end(); k_pos++)
	{
		t_Key* pKey = new t_Key;
		pKey->szComment = (*k_pos).szComment;
		pKey->szKey = (*k_pos).szKey;
		pKey->szValue = (*k_pos).szValue;

		pSection->Keys.push_back(*pKey);
	}

	m_Sections.push_back(*pSection);
	m_bDirty = true;

	return true;
}

// HasSection
// Returns true if the specified section exists.
bool cdf::CDataFile::HasSection(const t_Str &szSection)
{
	return GetSection(szSection) != NULL;
}

// SectionCount
// Simply returns the number of sections in the list.
int cdf::CDataFile::SectionCount()
{
	return m_Sections.size();
}

// KeyCount
// Returns the total number of keys contained within all the sections.
int cdf::CDataFile::KeyCount()
{
	int nCounter = 0;
	SectionItor s_pos;

	for (s_pos = m_Sections.begin(); s_pos != m_Sections.end(); s_pos++)
		nCounter += (*s_pos).Keys.size();

	return nCounter;
}


// Protected Member Functions ///////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// GetKey
// Given a key and section name, looks up the key and if found, returns a
// pointer to that key, otherwise returns NULL.
t_Key*	cdf::CDataFile::GetKey(const t_Str &szKey, const t_Str &szSection)
{
	KeyItor k_pos;
	t_Section* pSection;

	// Since our default section has a name value of t_Str("") this should
	// always return a valid section, wether or not it has any keys in it is
	// another matter.
	if ( (pSection = GetSection(szSection)) == NULL )
		return NULL;

	for (k_pos = pSection->Keys.begin(); k_pos != pSection->Keys.end(); k_pos++)
	{
		if ( CompareNoCase( (*k_pos).szKey, szKey ) == 0 )
			return (t_Key*)&(*k_pos);
	}

	return NULL;
}

// GetSection
// Given a section name, locates that section in the list and returns a pointer
// to it. If the section was not found, returns NULL
t_Section* cdf::CDataFile::GetSection(const t_Str &szSection)
{
	SectionItor s_pos;

	for (s_pos = m_Sections.begin(); s_pos != m_Sections.end(); s_pos++)
	{
		if ( CompareNoCase( (*s_pos).szName, szSection ) == 0 )
			return (t_Section*)&(*s_pos);
	}

	return NULL;
}


t_Str cdf::CDataFile::CommentStr(t_Str szComment)
{
	t_Str szNewStr = t_Str("");

	Trim(szComment);

		if ( szComment.size() == 0 )
			return szComment;

	if ( szComment.find_first_of(CommentIndicators) != 0 )
	{
		szNewStr = CommentIndicators[0];
		szNewStr += " ";
	}

	szNewStr += szComment;

	return szNewStr;
}



// Utility Functions ////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////////////////////////

// GetNextWord
// Given a key +delimiter+ value string, pulls the key name from the string,
// deletes the delimiter and alters the original string to contain the
// remainder.  Returns the key
t_Str cdf::GetNextWord(t_Str& CommandLine)
{
	int nPos = CommandLine.find_first_of(EqualIndicators);
	t_Str sWord = t_Str("");

	if ( nPos > -1 )
	{
		sWord = CommandLine.substr(0, nPos);
		CommandLine.erase(0, nPos+1);
	}
	else
	{
		sWord = CommandLine;
		CommandLine = t_Str("");
	}

	Trim(sWord);
	return sWord;
}


// CompareNoCase
// it's amazing what features std::string lacks.  This function simply
// does a lowercase compare against the two strings, returning 0 if they
// match.
int cdf::CompareNoCase(const t_Str& str1, const t_Str& str2)
{
#ifdef WIN32
	return stricmp(str1.c_str(), str2.c_str());
#else
	return strcasecmp(str1.c_str(), str2.c_str());
#endif
}

// Trim
// Trims whitespace from both sides of a string.
void cdf::Trim(t_Str& szStr)
{
	t_Str szTrimChars = WhiteSpace;

	szTrimChars += EqualIndicators;
	int nPos, rPos;

	// trim left
	nPos = szStr.find_first_not_of(szTrimChars);

	if ( nPos > 0 )
		szStr.erase(0, nPos);

	// trim right and return
	nPos = szStr.find_last_not_of(szTrimChars);
	rPos = szStr.find_last_of(szTrimChars);

	if ( rPos > nPos && rPos > -1)
		szStr.erase(rPos, szStr.size()-rPos);
}

// WriteLn
// Writes the formatted output to the file stream, returning the number of
// bytes written.
int cdf::WriteLn(std::ofstream &stream, const char *fmt, ...)
{
	char buf[MAX_BUFFER_LEN];
	int nLength;

	memset(buf, 0, MAX_BUFFER_LEN);
	va_list args;

	va_start (args, fmt);
	  nLength = vsnprintf(buf, MAX_BUFFER_LEN, fmt, args);
	va_end (args);


	if ( buf[nLength] != '\n' && buf[nLength] != '\r' )
		buf[nLength++] = '\n';


	stream.write(buf, nLength);

	return nLength;
}

// Report
// A simple reporting function. Outputs the report messages to stdout
// This is a dumb'd down version of a simmilar function of mine, so if
// it looks like it should do more than it does, that's why...
void cdf::Report(e_DebugLevel DebugLevel, const char *fmt, ...)
{
	char buf[MAX_BUFFER_LEN];
	int nLength;
	t_Str szMsg;

	va_list args;

	memset(buf, 0, MAX_BUFFER_LEN);

	va_start (args, fmt);
	  nLength = vsnprintf(buf, MAX_BUFFER_LEN, fmt, args);
	va_end (args);


	if ( buf[nLength] != '\n' && buf[nLength] != '\r' )
		buf[nLength++] = '\n';


	switch ( DebugLevel )
	{
		case E_DEBUG:
			szMsg = "<debug> ";
			break;
		case E_INFO:
			szMsg = "<info> ";
			break;
		case E_WARN:
			szMsg = "<warn> ";
			break;
		case E_ERROR:
			szMsg = "<error> ";
			break;
		case E_FATAL:
			szMsg = "<fatal> ";
			break;
		case E_CRITICAL:
			szMsg = "<critical> ";
			break;
	}


	szMsg += buf;


#ifdef WIN32
	OutputDebugString(szMsg.c_str());
#endif

	printf(szMsg.c_str());

}
