#pragma once
typedef struct mimetype_s
{
	char extension[10];
	char mimetype[40];
} mimetype_t;

char *ReturnMimeType ( char * );

