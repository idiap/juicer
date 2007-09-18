/*
 * Copyright 2003 by IDIAP Research Institute
 *                   http://www.idiap.ch
 *
 * See the file COPYING for the licence associated with this software.
 */

// Modified for use without the rest of torch.
// Changed name from DiskXFile to DiskFile.
// Standalone - no longer derived from XFile.
// Also make nativeMode stuff non-static so each instance can be
//   configured to write in either big or little endian.
// Darren Moore (moore@idiap.ch) 17/01/04


#ifndef DISK_FILE_INC
#define DISK_FILE_INC

#include "general.h"

namespace Juicer {


class DiskFile
{
private:
	bool		is_native_mode;
	char		*buffer_block;
	int		buffer_block_size;
	void		reverseMemory(void *ptr_, int block_size, int n_blocks);

public:
	FILE		*file;
	bool		is_opened;
	bool		its_a_pipe;

	/// Open "file_name" with the flags #open_flags#
	DiskFile( const char *file_name , const char *open_flags , bool bigEndian ) ;

	/// Use the given file...
	DiskFile( FILE *file_ , bool bigEndian ) ;

	//-----

	/// Returns #true# if the processor uses the little endian coding format.
	static bool isLittleEndianProcessor();

	/// Returns #true# if the processor uses the big endian coding format.
	static bool isBigEndianProcessor();

	/// Returns #true# if we'll load/save using the native mode.
	bool isNativeMode() ;

	/** We'll load/save using native mode.
	  We use little endian iff the computer uses little endian.
	  We use big endian iff the computer uses big endian.
	 */
	void setNativeMode() ;

	/** We'll load/save using little endian mode.
	  It means that if the computer doesn't use Little Endian,
	  data will be converted.
	 */
	void setLittleEndianMode();

	/** We'll load/save using big endian mode.
	  It means that if the computer doesn't use Big Endian,
	  data will be converted.
	 */
	void setBigEndianMode();

	//-----

	int read(void *ptr, int block_size, int n_blocks);
	int write(void *ptr, int block_size, int n_blocks);
	int eof();
	int flush();
	int seek(long offset, int whence);
	long tell();
	void rewind();
	int printf(const char *format, ...);
	int scanf(const char *format, void *ptr) ;
	char *gets(char *dest, int size_);

	//-----

	virtual ~DiskFile();
};

}

#endif
