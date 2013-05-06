#ifndef CREATE_ACTION_H
#define CREATE_ACTION_H

#include "action.h"
#include "tree_builder.h"
#include <memory>
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

    TreeBuilder m_tree_builder;
    std::unique_ptr<JT::ObjectNode> m_out_tree;
    std::string m_out_file_name;
    int m_out_file;
};

#endif //CREATE_ACTION_H
