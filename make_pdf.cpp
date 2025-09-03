#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <iomanip>
#include <windows.h>
using namespace std;

// 每页内容
struct PageContent {
    string text;
};

// 简单估算每个字符的宽度（pt）
int charWidthEstimate(int fontSize) {
    return fontSize * 0.6; // 粗略估算：半个字号宽
}

// 将一行文本按页面宽度拆分成多行
vector<string> wrapLine(const string &line, int fontSize, int pageWidth) {
    vector<string> result;
    int maxChars = pageWidth / charWidthEstimate(fontSize);
    size_t start = 0;
    while (start < line.size()) {
        result.push_back(line.substr(start, maxChars));
        start += maxChars;
    }
    return result;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    // 语言选择
    cout << "请选择语言 / Please select language:\n";
    cout << "1: 中文\n2: English\n";
    string langChoice;
    getline(cin, langChoice);
    bool isChinese = (langChoice == "1");

    string promptFilename = isChinese ? "请输入输出 PDF 文件名 (例如 hello.pdf): "
                                      : "Enter output PDF filename (e.g., hello.pdf): ";
    string promptFontSize = isChinese ? "请输入字体大小 (整数, 例如 24): "
                                      : "Enter font size (integer, e.g., 24): ";
    string promptLineSpacing = isChinese ? "请输入行距 (整数, 字号加上此数值即每行间距, 例如 10): "
                                        : "Enter line spacing (integer, added to font size, e.g., 10): ";
    string promptShowPage = isChinese ? "是否在每页显示页码？(y/n): "
                                      : "Show page numbers on each page? (y/n): ";
    string editInfo = isChinese ? "\n编辑模式: 输入多行文字，每行按回车\n- 输入 SAVE 或 :wq 保存并生成 PDF\n- 输入 QUIT 放弃编辑退出程序\n\n"
                                : "\nEditing mode: Enter multiple lines, press Enter after each line\n- Type SAVE or :wq to save and generate PDF\n- Type QUIT to exit without saving\n\n";
    string systemSaving = isChinese ? "[系统] 保存命令，生成 PDF...\n" : "[System] Save command detected, generating PDF...\n";
    string systemQuit = isChinese ? "[系统] 放弃编辑，退出程序。\n" : "[System] Quit without saving.\n";
    string errorCannotCreate = isChinese ? "[错误] 无法创建 PDF 文件\n" : "[Error] Cannot create PDF file\n";
    string successSaved = isChinese ? "[成功] PDF 已保存到: "
                                    : "[Success] PDF saved to: ";

    cout << (isChinese ? "===== 简易 PDF 编辑器 =====\n" : "===== Simple PDF Editor =====\n");

    // 输出 PDF 文件名
    cout << promptFilename;
    string filename;
    getline(cin, filename);

    // 字体大小
    cout << promptFontSize;
    int fontSize;
    cin >> fontSize;
    cin.ignore();

    // 行距
    cout << promptLineSpacing;
    int lineSpacing;
    cin >> lineSpacing;
    cin.ignore();

    // 是否显示页码
    cout << promptShowPage;
    string showPageStr;
    getline(cin, showPageStr);
    bool showPageNum = (showPageStr == "y" || showPageStr == "Y");

    // 页面宽度（点数）
    int pageWidth = 500;
    int linesPerPage = 40;
    int yStart = 800;
    int yStep = fontSize + lineSpacing; // 每行 Y 间距由字号加行距决定

    cout << editInfo;

    // 输入文本
    vector<string> lines;
    string line;
    int lineNum = 1;
    while (true) {
        cout << lineNum << "> ";
        getline(cin, line);

        if (line == "SAVE" || line == ":wq") {
            cout << systemSaving;
            break;
        }
        if (line == "QUIT") {
            cout << systemQuit;
            return 0;
        }

        // 自动换行拆分
        vector<string> wrapped = wrapLine(line, fontSize, pageWidth);
        for (auto &l : wrapped) lines.push_back(l);

        // 自动分页提示
        if (lines.size() % linesPerPage == 0) {
            if (isChinese)
                cout << "[系统] 自动分页: 第" << (lines.size() / linesPerPage + 1) << "页开始\n";
            else
                cout << "[System] Auto page break: page " << (lines.size() / linesPerPage + 1) << " starts\n";
        }

        lineNum++;
    }

    ofstream out(filename, ios::binary);
    if (!out) {
        cerr << errorCannotCreate;
        return 1;
    }

    out << "%PDF-1.4\n";
    vector<long> offsets;
    offsets.push_back(0); // obj 0 占位

    // 按页切分
    vector<PageContent> pages;
    for (size_t i = 0; i < lines.size(); i += linesPerPage) {
        ostringstream ss;
        int y = yStart;
        size_t end = min(i + linesPerPage, lines.size());
        for (size_t j = i; j < end; j++) {
            ss << "BT /F1 " << fontSize << " Tf 50 " << y << " Td ("
               << lines[j] << ") Tj ET\n";
            y -= yStep;
        }
        // 页码显示
        if (showPageNum) {
            int pageIndex = i / linesPerPage + 1;
            ss << "BT /F1 " << fontSize << " Tf 500 20 Td ("
               << (isChinese ? "第" : "") << pageIndex 
               << (isChinese ? "页" : "") << ") Tj ET\n";
        }
        pages.push_back({ss.str()});
    }

    int objNum = 1;
    int catalogObj = objNum++;
    int pagesObj = objNum++;
    vector<int> pageObjs, contentObjs;
    int fontObj = objNum++;

    // Font 对象
    offsets.push_back(out.tellp());
    out << fontObj << " 0 obj\n"
        << "<< /Type /Font /Subtype /Type1 /BaseFont /Helvetica >>\n"
        << "endobj\n";

    // 每页对象
    for (size_t i = 0; i < pages.size(); i++) {
        int pageObj = objNum++;
        int contentObj = objNum++;
        pageObjs.push_back(pageObj);
        contentObjs.push_back(contentObj);

        // Page 对象
        offsets.push_back(out.tellp());
        out << pageObj << " 0 obj\n"
            << "<< /Type /Page /Parent " << pagesObj << " 0 R "
            << "/MediaBox [0 0 595 842] "
            << "/Contents " << contentObj << " 0 R "
            << "/Resources << /Font << /F1 " << fontObj << " 0 R >> >> >>\n"
            << "endobj\n";

        // Content 对象
        offsets.push_back(out.tellp());
        out << contentObj << " 0 obj\n"
            << "<< /Length " << pages[i].text.size() << " >>\n"
            << "stream\n"
            << pages[i].text
            << "endstream\n"
            << "endobj\n";
    }

    // Pages 对象
    offsets.push_back(out.tellp());
    out << pagesObj << " 0 obj\n"
        << "<< /Type /Pages /Kids [";
    for (int pid : pageObjs) out << pid << " 0 R ";
    out << "] /Count " << pages.size() << " >>\n"
        << "endobj\n";

    // Catalog 对象
    offsets.push_back(out.tellp());
    out << catalogObj << " 0 obj\n"
        << "<< /Type /Catalog /Pages " << pagesObj << " 0 R >>\n"
        << "endobj\n";

    // xref
    long xrefPos = out.tellp();
    out << "xref\n0 " << objNum << "\n";
    out << "0000000000 65535 f \n";
    for (int i = 1; i < objNum; i++) {
        out << setw(10) << setfill('0') << offsets[i]
            << " 00000 n \n";
    }

    // trailer
    out << "trailer\n"
        << "<< /Size " << objNum << " /Root " << catalogObj << " 0 R >>\n"
        << "startxref\n" << xrefPos << "\n%%EOF\n";

    out.close();
    cout << "\n" << successSaved << filename << "\n";
    return 0;
}
