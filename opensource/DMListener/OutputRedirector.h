#pragma once
#include <windows.h>
#include <streambuf>
#include <string>

class OutputRedirector {
public:
    explicit OutputRedirector(HWND hEdit);
    ~OutputRedirector();

private:

    class NarrowBuf : public std::streambuf {
    public:
        explicit NarrowBuf(HWND hEdit);
    protected:
        
        virtual std::streambuf::int_type overflow(std::streambuf::int_type ch) override;
        virtual int sync() override;
    private:
        void append(const std::wstring& text);
        HWND m_hEdit;
        std::string buffer;
    };

    // ���ַ�������
    class WideBuf : public std::wstreambuf {
    public:
        explicit WideBuf(HWND hEdit);
    protected:
     
        virtual std::wstreambuf::int_type overflow(std::wstreambuf::int_type ch) override;
        virtual int sync() override;
    private:
        void append(const std::wstring& text);
        HWND m_hEdit;
        std::wstring buffer;
    };

    NarrowBuf* m_narrowBuf;
    WideBuf* m_wideBuf;
    std::streambuf* m_oldCoutBuf;
    std::wstreambuf* m_oldWcoutBuf;
};
