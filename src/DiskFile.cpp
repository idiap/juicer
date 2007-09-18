/*
 * Copyright 2003 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

#include "DiskFile.h"
#ifdef _MSC_VER
#include <fcntl.h>
#endif

using namespace Torch;

namespace Juicer {


DiskFile::DiskFile( const char *file_name , const char *open_flags , bool bigEndian )
{
#ifdef _MSC_VER
	_fmode = _O_BINARY;
#endif

	its_a_pipe = false ;
	bool zipped_file = false;
	if(!strcmp(open_flags, "r"))
	{
		if(strlen(file_name) > 3)
		{
			if(strcmp(file_name+strlen(file_name)-3, ".gz"))
				zipped_file = false;
			else
				zipped_file = true;
		}
		else
			zipped_file = false;

		if(zipped_file)
		{
			char cmd_buffer[1000] ;
			sprintf( cmd_buffer , "zcat %s" , file_name ) ;
			file = fopen(file_name, "r");
			if(!file)
				error("DiskFile: cannot open the file <%s> for reading", file_name);
			fclose(file);

			file = popen(cmd_buffer, open_flags);
			if(!file)
				error("DiskFile: cannot execute the command <%s>", file_name, cmd_buffer);
			free(cmd_buffer);
		}
	}

	if(!zipped_file)
	{
		file = fopen(file_name, open_flags);
		if(!file)
			error("DiskFile: cannot open <%s> in mode <%s>. Sorry.", file_name, open_flags);
	}
	is_opened = true;

	// Buffer
	buffer_block = NULL;
	buffer_block_size = 0;

	if ( bigEndian == true )
		setBigEndianMode() ;
	else
		setLittleEndianMode() ;
}

DiskFile::DiskFile( FILE *file_ , bool bigEndian )
{
	file = file_;
	is_opened = false;
	its_a_pipe = false;

	// Buffer
	buffer_block = NULL;
	buffer_block_size = 0;

	if ( bigEndian == true )
		setBigEndianMode() ;
	else
		setLittleEndianMode() ;
}

int DiskFile::read(void *ptr, int block_size, int n_blocks)
{
	int melanie = fread(ptr, block_size, n_blocks, file);

	if(!is_native_mode)
		reverseMemory(ptr, block_size, n_blocks);

	return(melanie);
}

int DiskFile::write(void *ptr, int block_size, int n_blocks)
{
	if(!is_native_mode)
		reverseMemory(ptr, block_size, n_blocks);

	int melanie = fwrite(ptr, block_size, n_blocks, file);

	if(!is_native_mode)
		reverseMemory(ptr, block_size, n_blocks);

	return(melanie);
}

int DiskFile::eof()
{
	return(feof(file));
}

int DiskFile::flush()
{
	return(fflush(file));
}

int DiskFile::seek(long offset, int whence)
{
	return(fseek(file, offset, whence));
}

long DiskFile::tell()
{
	return(ftell(file));
}

void DiskFile::rewind()
{
	::rewind(file);
}

int DiskFile::printf(const char *format, ...)
{
	va_list args;
	va_start(args, format);
	int res = vfprintf(file, format, args);
	va_end(args);
	return(res);
}

int DiskFile::scanf(const char *format, void *ptr)
{
	int res = fscanf(file, format, ptr);
	return(res);
}

char *DiskFile::gets(char *dest, int size_)
{
	return(fgets(dest, size_, file));
}

//-----

bool DiskFile::isLittleEndianProcessor()
{
	int x = 7;
	char *ptr = (char *)&x;

	if(ptr[0] == 0)
		return(false);
	else
		return(true);
}

bool DiskFile::isBigEndianProcessor()
{
	return(!isLittleEndianProcessor());
}

bool DiskFile::isNativeMode()
{
	return(is_native_mode);
}

void DiskFile::setNativeMode()
{
	is_native_mode = true;
}

void DiskFile::setLittleEndianMode()
{
	if(isLittleEndianProcessor())
		is_native_mode = true;
	else
		is_native_mode = false;
}

void DiskFile::setBigEndianMode()
{
	if(isBigEndianProcessor())
		is_native_mode = true;
	else
		is_native_mode = false;
}

void DiskFile::reverseMemory(void *ptr_, int block_size, int n_blocks)
{
	if(block_size == 1)
		return;

	char *ptr = (char *)ptr_;
	char *ptrr, *ptrw;

	if( block_size > buffer_block_size )
	{
		buffer_block = (char *)realloc( buffer_block , block_size ) ;
		buffer_block_size = block_size ;
	}

	for(int i = 0; i < n_blocks; i++)
	{
		ptrr = ptr + ((i+1)*block_size);
		ptrw = buffer_block;

		for(int j = 0; j < block_size; j++)
		{
			ptrr--;
			*ptrw++ = *ptrr;
		}

		ptrr = buffer_block;
		ptrw = ptr + (i*block_size);
		for(int j = 0; j < block_size; j++)
			*ptrw++ = *ptrr++;
	}
}

//-----

DiskFile::~DiskFile()
{
	if(is_opened)
	{
		if(its_a_pipe)
			pclose(file);
		else
			fclose(file);
	}
}

}
