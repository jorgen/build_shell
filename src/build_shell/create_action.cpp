#include "create_action.h"

#include <sys/types.h>
#include <sys/dir.h>
#include <errno.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <libgen.h>

#include "json_tree.h"

CreateAction::CreateAction(const Configuration &configuration,
        const std::string &outfile)
    : Action(configuration)
    , m_error(false)
{

    if (m_configuration.buildsetFile().size()) {
        m_buildset_in.setFile(m_configuration.buildsetFile());
        const char *buildset_in_data = static_cast<const char *>(m_buildset_in.map());

        if (buildset_in_data) {
            JT::TreeBuilder tree_builder;
            tree_builder.create_root_if_needed = true;
            auto tree_build = tree_builder.build(buildset_in_data,m_buildset_in.size());
            if (tree_build.second == JT::Error::NoError) {
                m_out_tree = tree_build.first->asObjectNode();
            }
        }
    }

    if (!m_out_tree)
        m_out_tree = new JT::ObjectNode();

    fprintf(stderr, "FOEWOFJWEJ\n");
    const std::string *actual_outfile = &outfile;
    if (!actual_outfile->size())
        actual_outfile = &configuration.buildsetOutFile();

    if (actual_outfile->size()) {
        fprintf(stderr, "Actual out file %s\n", actual_outfile->c_str());
        m_out_file_name = *actual_outfile;
        mode_t create_mode = S_IROTH | S_IRGRP | S_IRUSR | S_IWUSR;
        m_out_file = open(m_out_file_name.c_str(), O_RDWR | O_CLOEXEC | O_CREAT | O_EXCL, create_mode);
        if (m_out_file < 0) {
            fprintf(stderr, "Failed to open buildset Out File %s\n%s\n",
                    m_out_file_name.c_str(), strerror(errno));
            m_error = true;
        }
    } else {
        m_out_file = STDOUT_FILENO;
    }
}

CreateAction::~CreateAction()
{
    if (m_out_file_name.size())
        close(m_out_file);
}

struct serialize_buffer
{
    int file;
    char buffer[4096];
    bool error = false;

    void writeOutBuffer(const JT::SerializerBuffer &buffer)
    {
        size_t written = write(file,buffer.buffer,buffer.used);
        if (written < buffer.used) {
            fprintf(stderr, "Error while writing to outbuffer: %s\n", strerror(errno));
            error = true;
        }
    }


    void request_flush(JT::Serializer *serializer)
    {
        if (serializer->buffers().size()) {
            JT::SerializerBuffer serializer_buffer = serializer->buffers().front();
            serializer->clearBuffers();

            writeOutBuffer(serializer_buffer);
        }
        serializer->appendBuffer(buffer,sizeof buffer);
    }
};

static bool fill_file_with_node(JT::ObjectNode *node, int file)
{
    if (lseek(file,0,SEEK_SET))
        return false;

    serialize_buffer buffer;
    buffer.file = file;

    std::function<void(JT::Serializer *)> callback =
        std::bind(&serialize_buffer::request_flush, &buffer, std::placeholders::_1);


    JT::TreeSerializer serializer;
    JT::SerializerOptions options = serializer.options();
    options.setPretty(true);
    serializer.setOptions(options);
    serializer.addRequestBufferCallback(callback);
    serializer.serialize(node);

    if (serializer.buffers().size()) {
        JT::SerializerBuffer serializer_buffer = serializer.buffers().front();
        buffer.writeOutBuffer(serializer_buffer);
    }

    const char newline[] = "\n";
    write(file, newline, sizeof newline);

   return true;
}

bool CreateAction::execute()
{
    if (m_error)
        return false;

    DIR *source_dir = opendir(m_configuration.srcDir().c_str());
    if (!source_dir) {
        fprintf(stderr, "Failed to open directory: %s\n%s\n",
                m_configuration.srcDir().c_str(), strerror(errno));
        return false;
    }
    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));

    if (chdir(m_configuration.srcDir().c_str()) != 0) {
        fprintf(stderr, "Could not change dir: %s\n%s\n",
                m_configuration.srcDir().c_str(),strerror(errno));
        return false;
    }

    while (struct dirent *ent = readdir(source_dir)) {
        if (strncmp(".",ent->d_name, sizeof(".")) == 0 ||
            strncmp("..", ent->d_name, sizeof("..")) == 0)
            continue;
        struct stat buf;
        if (stat(ent->d_name, &buf) != 0) {
            fprintf(stderr, "Something whent wrong when stating file %s: %s\n",
                    ent->d_name, strerror(errno));
            continue;
        }
        if (S_ISDIR(buf.st_mode)) {
            chdir(ent->d_name);
            if (!handleCurrentSrcDir()) {
                fprintf(stderr, "Failed to handle dir %s\n", ent->d_name);
                return false;
            }
            chdir(m_configuration.srcDir().c_str());
        }
    }

    //this should not fail ;)
    chdir(cwd);

    fill_file_with_node(m_out_tree, m_out_file);
    return true;
}


bool CreateAction::handleCurrentSrcDir()
{
    std::string tmp_file;
    int tmp_file_desc = Configuration::createTempFileFromCWD(tmp_file);
    if (tmp_file_desc <= 0)
        return false;


    char cwd[PATH_MAX];
    getcwd(cwd, sizeof(cwd));
    const char *base_name = basename(cwd);
    JT::ObjectNode *root_for_dir = m_out_tree->objectNodeAt(base_name);
    if (!root_for_dir) {
        root_for_dir = new JT::ObjectNode();
    }

    fill_file_with_node(root_for_dir, tmp_file_desc);
    close(tmp_file_desc);

    int exit_code = 0;
    if (access(".git", X_OK) == 0) {
        exit_code = m_configuration.runScript("state_git.sh", tmp_file.c_str());
    } else if (access(".svn", X_OK) == 0) {
        exit_code = m_configuration.runScript("state_regular_dir.sh", tmp_file.c_str());
    } else {
        exit_code = m_configuration.runScript("state_regular_dir.sh", tmp_file.c_str());
    }

    tmp_file_desc = open(tmp_file.c_str(), O_RDONLY|O_CLOEXEC);
    if (tmp_file_desc < 0) {
        fprintf(stderr, "Suspecting script for messing up temporary file: %s\n%s\n",
                tmp_file.c_str(), strerror(errno));
        return false;
    }
    MmappedReadFile tmp_file_mmap_handler;
    tmp_file_mmap_handler.setFile(tmp_file_desc);

    const char *temp_file_data = static_cast<const char *>(tmp_file_mmap_handler.map());

    JT::TreeBuilder tree_builder;
    tree_builder.create_root_if_needed = true;
    auto tree_build = tree_builder.build(temp_file_data, tmp_file_mmap_handler.size());
    if (tree_build.second == JT::Error::NoError) {
        JT::Property property(JT::Token::String, JT::Data(base_name, strlen(base_name), true));
        JT::ObjectNode *root_for_cwd_dir = tree_build.first->asObjectNode();
        m_out_tree->insertNode(property, root_for_cwd_dir, true);
    } else {
        fprintf(stderr, "Failed to understand output from %s\n", base_name);
    }
    close(tmp_file_desc);
    unlink(tmp_file.c_str());
    return exit_code == 0;
}

