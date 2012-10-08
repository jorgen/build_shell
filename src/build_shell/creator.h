#ifndef CREATOR_H
#define CREATOR_H

#include <string>

class Configuration;

class Creator
{
public:
    Creator(Configuration *configuration);

    void writeNewBuildSet(std::string file);
private:
    void handleCurrentSrcDir();

    std::string m_src_dir;
    std::string m_build_set_file_in;
};

#endif //CREATOR_H
