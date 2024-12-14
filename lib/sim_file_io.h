#ifndef SIM_FILE_IO_H
#define SIM_FILE_IO_H
#include <matio.h>
#include <string>

bool checkFileType(const std::string& filepath);

matvar_t* handle_mat_file(mat_t* matfile);


#endif //SIM_FILE_IO_H
