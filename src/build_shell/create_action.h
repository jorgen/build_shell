#ifndef CREATE_ACTION_H
#define CREATE_ACTION_H

#include "action.h"
#include "mmapped_file.h"

namespace JT {
    class ObjectNode;
}

class CreateAction : public Action
{
public:
    CreateAction(const Configuration &configuration,
            const std::string &outfile = std::string());
    ~CreateAction();

    bool execute();

private:
    bool handleCurrentSrcDir();

    MmappedReadFile m_buildset_in;
    JT::ObjectNode *m_out_tree;
    std::string m_out_file_name;
    int m_out_file;
    bool m_error;
};

#endif //CREATE_ACTION_H
