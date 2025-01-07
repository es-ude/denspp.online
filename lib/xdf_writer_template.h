#ifndef XDF_WRITER_TEMPLATE_H
#define XDF_WRITER_TEMPLATE_H

#include "xdfwriter.h"
#include "config.h"
void write_header(XDFWriter* writer, const Config& cfg );

void write_footer(XDFWriter* writer,const Config& cfg ,double exact_ts, long time_stamp);

#endif //XDF_WRITER_TEMPLATE_H
