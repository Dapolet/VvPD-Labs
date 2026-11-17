#include <cstdio>
#include <iostream>
#include <memory>
#include <utility>
#include <vector>

class FileHandle {
public:
    FileHandle(const char *path, const char *mode)
        : file_(std::fopen(path, mode))
    {
        std::cout << "FileHandle: "
                  << (file_ ? "opened " : "failed to open ") << path << '\n';
    }

    ~FileHandle()
    {
        if (file_ != nullptr) {
            std::fclose(file_);
            std::cout << "FileHandle: closed in destructor\n";
        }
    }

    FileHandle(const FileHandle &) = delete;
    FileHandle &operator=(const FileHandle &) = delete;

    FileHandle(FileHandle &&other) noexcept
        : file_(other.file_)
    {
        other.file_ = nullptr;
    }

    std::FILE *get() const { return file_; }
    explicit operator bool() const { return file_ != nullptr; }

private:
    std::FILE *file_;
};

int main()
{
    std::cout << "=== std::vector владеет динамическим массивом ===\n";
    {
        std::vector<int> data(5);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<int>(i * i);

        int sum = 0;
        for (int value : data)
            sum += value;

        std::cout << "size = " << data.size()
                  << ", capacity = " << data.capacity()
                  << ", sum = " << sum << '\n';

        data.push_back(25);
        std::cout << "after push_back: size = " << data.size()
                  << ", capacity = " << data.capacity() << '\n';
    }

    std::cout << "\n=== std::unique_ptr --- единственный владелец ===\n";
    {
        auto value = std::make_unique<int>(304);
        std::cout << "*value = " << *value << '\n';

        auto owner = std::move(value);
        std::cout << "after move: original = "
                  << (value ? "not null" : "null")
                  << ", owner = " << *owner << '\n';
    }

    std::cout << "\n=== RAII для FILE * ===\n";
    {
        FileHandle file("raii_demo.txt", "w");
        if (file) {
            std::fputs("RAII demo\n", file.get());
        }
    }

    std::remove("raii_demo.txt");
    std::cout << "\nscope ended: vector, unique_ptr and FileHandle "
                 "released automatically\n";
    return 0;
}
