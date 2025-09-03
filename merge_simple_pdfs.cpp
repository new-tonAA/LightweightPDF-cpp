// merge_simple_pdfs_fixed.cpp
// 简单 PDF 合并器（不依赖外部库，修复“第一页重复/不合并第二个文档”问题）
// 编译: g++ -std=c++17 merge_simple_pdfs_fixed.cpp -o merge_simple_pdfs_fixed

#include <bits/stdc++.h>
#include <windows.h>
using namespace std;

// PDF 对象结构
struct PdfObj {
    int id = 0;
    int gen = 0;
    string raw;
    string body;
};

// 读取整个文件
string readFileAll(const string &path) {
    ifstream in(path, ios::binary);
    if (!in) return "";
    ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

// 解析 obj ... endobj
map<int, PdfObj> parseObjects(const string &data) {
    map<int, PdfObj> objs;
    regex reObj(R"((\d+)\s+(\d+)\s+obj\b)");
    auto it = sregex_iterator(data.begin(), data.end(), reObj);
    auto it_end = sregex_iterator();
    for (; it != it_end; ++it) {
        smatch m = *it;
        int id = stoi(m[1].str());
        int gen = stoi(m[2].str());
        size_t pos = m.position(0);
        size_t start_body = pos + m.length(0);
        size_t endobj_pos = data.find("endobj", start_body);
        if (endobj_pos == string::npos) continue;
        size_t full_end = endobj_pos + 6; // strlen("endobj") = 6
        string raw = data.substr(pos, full_end - pos);
        string body = data.substr(start_body, endobj_pos - start_body);
        while (!body.empty() && (body.front() == '\n' || body.front() == '\r')) body.erase(body.begin());
        PdfObj o{id, gen, raw, body};
        objs[id] = o;
    }
    return objs;
}

// 找页面对象
vector<int> findPageObjects(const map<int,PdfObj>& objs) {
    vector<int> pages;

    // [FIX] 关键修复：此前用 string::find("/Type /Page") 会把 "/Type /Pages" 也误判为页面
    // 使用正则 "/Type\s*/Page\b" 严格匹配“Page”单词边界，不会匹配到 "Pages"
    regex rePage(R"(/Type\s*/Page\b)");

    for (auto &p: objs) {
        const PdfObj &o = p.second;
        if (regex_search(o.body, rePage)) pages.push_back(o.id);
    }
    sort(pages.begin(), pages.end());

    // [SAFE] 去重（以防输入里出现重复定义或奇怪的结构）
    pages.erase(unique(pages.begin(), pages.end()), pages.end());
    return pages;
}

// 查找引用（间接对象号）
set<int> referencedObjects(const string &body) {
    set<int> refs;
    regex reRef(R"((\d+)\s+0\s+R\b)");
    sregex_iterator it(body.begin(), body.end(), reRef);
    sregex_iterator it_end;
    for (; it != it_end; ++it) {
        smatch m = *it;
        int id = stoi(m.str(1));
        refs.insert(id);
    }
    return refs;
}

// 收集依赖对象（不包含 Page 自身）
// [FIX] 继续保持“不把 Page 放进依赖集合”，避免任何途径的“页面对象重复加入”
set<int> collectNonPageDependencies(const map<int,PdfObj>& objs, const vector<int>& pageIds) {
    set<int> pageSet(pageIds.begin(), pageIds.end());
    set<int> collected;
    queue<int> q;
    for (int pid : pageIds) {
        if (objs.count(pid)) q.push(pid);
    }
    while (!q.empty()) {
        int cur = q.front(); q.pop();
        auto it = objs.find(cur);
        if (it == objs.end()) continue;
        const string &body = it->second.body;
        set<int> refs = referencedObjects(body);
        for (int rid : refs) {
            if (rid == 0) continue;
            if (pageSet.count(rid)) continue;        // 不收集 Page
            if (!objs.count(rid)) continue;          // 外部或缺失
            if (!collected.count(rid)) {
                collected.insert(rid);
                q.push(rid);
            }
        }
    }
    return collected;
}

// 重映射引用（把 body 内的 "oldid 0 R" 换成 "newid 0 R"）
string remapReferences(const string &body, const map<int,int>& idmap) {
    string out;
    out.reserve(body.size());
    regex reRef(R"((\d+)\s+0\s+R\b)");
    sregex_iterator it(body.begin(), body.end(), reRef);
    sregex_iterator it_end;
    size_t last = 0;
    for (; it != it_end; ++it) {
        smatch m = *it;
        int oldid = stoi(m.str(1));
        size_t pos = m.position(0);
        out.append(body.substr(last, pos - last));
        auto f = idmap.find(oldid);
        if (f != idmap.end()) {
            out.append(to_string(f->second));
            out.append(" 0 R");
        } else {
            out.append(m.str(0)); // 找不到映射则保持原样
        }
        last = pos + m.length(0);
    }
    out.append(body.substr(last));
    return out;
}

// 写 PDF
bool writeMergedPDF(const string &outpath,
                    const map<int,PdfObj>& allObjs,
                    const vector<int>& orderedObjs,
                    const vector<int>& pageIds) {
    map<int,int> idmap;
    int nextId = 1;
    for (int oid : orderedObjs) idmap[oid] = nextId++;
    int newPagesId = nextId++;
    int newCatalogId = nextId++;

    vector<pair<int,string>> outObjects;
    outObjects.reserve(idmap.size() + 2);

    // 先写非 Page 对象
    set<int> pageSet(pageIds.begin(), pageIds.end());
    for (int oid : orderedObjs) {
        if (pageSet.count(oid)) continue;
        auto it = allObjs.find(oid);
        if (it == allObjs.end()) continue; // [SAFE]
        int nid = idmap[oid];
        string remapped = remapReferences(it->second.body, idmap);
        ostringstream ss;
        ss << nid << " 0 obj\n" << remapped << "\nendobj\n";
        outObjects.push_back({nid, ss.str()});
    }

    // 再写 Page 对象（强制指向新的 Pages）
    for (int pid : pageIds) {
        auto it = allObjs.find(pid);
        if (it == allObjs.end()) continue; // [SAFE]
        int nid = idmap[pid];
        string remapped = remapReferences(it->second.body, idmap);

        // [FIX] 强制把 /Parent 指向新的 Pages；并且先移除旧的 /Parent，避免出现两个 /Parent
        {
            // 删除已有 /Parent X 0 R（如果有）
            regex reParent(R"(/Parent\s+\d+\s+0\s+R)");
            remapped = regex_replace(remapped, reParent, "");
            // 在字典末尾前插入新的 /Parent
            size_t pos = remapped.rfind(">>");
            if (pos != string::npos) {
                remapped.insert(pos, " /Parent " + to_string(newPagesId) + " 0 R ");
            } else {
                // [SAFE] 如果对象不是典型字典格式，兜底追加
                remapped += "\n/Parent " + to_string(newPagesId) + " 0 R\n";
            }
        }

        ostringstream ss;
        ss << nid << " 0 obj\n" << remapped << "\nendobj\n";
        outObjects.push_back({nid, ss.str()});
    }

    // Pages 对象
    ostringstream pagesStream;
    pagesStream << "<< /Type /Pages /Kids [";
    for (int pid : pageIds) {
        if (!idmap.count(pid)) {             // [SAFE] 防止 at 崩
            continue;
        }
        pagesStream << idmap.at(pid) << " 0 R ";
    }
    pagesStream << "] /Count " << pageIds.size() << " >>";
    ostringstream ss;
    ss << newPagesId << " 0 obj\n" << pagesStream.str() << "\nendobj\n";
    outObjects.push_back({newPagesId, ss.str()});

    // Catalog 对象
    ostringstream catalogStream;
    catalogStream << "<< /Type /Catalog /Pages " << newPagesId << " 0 R >>";
    ss.str(""); ss.clear();
    ss << newCatalogId << " 0 obj\n" << catalogStream.str() << "\nendobj\n";
    outObjects.push_back({newCatalogId, ss.str()});

    // 写入文件
    ofstream fout(outpath, ios::binary);
    if (!fout) return false;

    fout << "%PDF-1.4\n%âãÏÓ\n";
    vector<long> offsets;
    for (auto &p : outObjects) {
        offsets.push_back((long)fout.tellp());
        fout << p.second;
    }

    long xref_start = (long)fout.tellp();
    fout << "xref\n0 " << (outObjects.size() + 1) << "\n";
    fout << setw(10) << setfill('0') << 0 << " 65535 f \n";
    for (size_t i = 0; i < offsets.size(); ++i)
        fout << setw(10) << setfill('0') << offsets[i] << " 00000 n \n";
    fout << "trailer\n<< /Size " << (outObjects.size() + 1)
         << " /Root " << newCatalogId << " 0 R >>\n";
    fout << "startxref\n" << xref_start << "\n%%EOF\n";
    fout.close();
    return true;
}

int main() {
    SetConsoleOutputCP(CP_UTF8);

    cout << "请选择语言 / Choose language: 1-中文, 2-English: ";
    string langChoice;
    getline(cin, langChoice);
    bool isChinese = (langChoice == "1");

    string prompt1 = isChinese ? "请输入第一个 PDF 文件路径: " : "Enter first PDF path: ";
    string prompt2 = isChinese ? "请输入第二个 PDF 文件路径: " : "Enter second PDF path: ";
    string prompt3 = isChinese ? "请输入合并后 PDF 输出文件名: " : "Enter output PDF filename: ";
    string errorRead = isChinese ? "读取输入文件失败。" : "Failed to read input file.";
    string errorParse = isChinese ? "未能解析 PDF 对象，请确认输入文件是简单 PDF。" : "Failed to parse PDF objects, ensure simple PDFs.";
    string errorPages = isChinese ? "未找到页面对象。" : "No page objects found.";
    string successMsg = isChinese ? "合并完成，输出文件: " : "Merge complete, output file: ";

    string in1, in2, out;
    cout << prompt1; getline(cin, in1);
    cout << prompt2; getline(cin, in2);
    cout << prompt3; getline(cin, out);

    string d1 = readFileAll(in1);
    string d2 = readFileAll(in2);
    if (d1.empty() || d2.empty()) { cerr << errorRead << "\n"; return 2; }

    auto objs1 = parseObjects(d1);
    auto objs2 = parseObjects(d2);
    if (objs1.empty() || objs2.empty()) { cerr << errorParse << "\n"; return 3; }

    // [FIX] 严格找 Page（不把 Pages 当 Page）
    auto pages1 = findPageObjects(objs1);
    auto pages2 = findPageObjects(objs2);
    if (pages1.empty() && pages2.empty()) { cerr << errorPages << "\n"; return 4; }

    // 收集非 Page 依赖
    auto deps1 = collectNonPageDependencies(objs1, pages1);
    auto deps2 = collectNonPageDependencies(objs2, pages2);

    // 合并对象
    const int SHIFT = 1000000;
    map<int,PdfObj> combined;

    // 文档1：Page + 依赖
    for (int id : pages1) combined[id] = objs1[id];
    for (int id : deps1)  combined[id] = objs1[id];

    // 文档2：Page + 依赖（对象号整体平移）
    for (int id : pages2) combined[id + SHIFT] = objs2[id];
    for (int id : deps2)  combined[id + SHIFT] = objs2[id];

    // [FIX] 把文档2对象内部的引用改成平移后的 ID
    {
        regex reRef(R"((\d+)\s+0\s+R\b)");
        for (auto &kv : combined) {
            if (kv.first < SHIFT) continue;
            PdfObj &o = kv.second;
            string s = o.body, outStr;
            sregex_iterator it(s.begin(), s.end(), reRef), it_end;
            size_t last = 0;
            for (; it != it_end; ++it) {
                smatch m = *it;
                int refid = stoi(m.str(1));
                size_t pos = m.position(0);
                outStr.append(s.substr(last, pos - last));
                if (objs2.count(refid)) {
                    outStr.append(to_string(refid + SHIFT));
                    outStr.append(" 0 R");
                } else {
                    outStr.append(m.str(0));
                }
                last = pos + m.length(0);
            }
            outStr.append(s.substr(last));
            o.body = outStr;
        }
    }

    // 生成合并后页面列表（严格只包含 Page 对象）
    vector<int> pageIds;
    for (int p : pages1) pageIds.push_back(p);
    for (int p : pages2) pageIds.push_back(p + SHIFT);

    // [SAFE] 再次去重
    sort(pageIds.begin(), pageIds.end());
    pageIds.erase(unique(pageIds.begin(), pageIds.end()), pageIds.end());

    // orderedObjs：先非 Page，再 Page
    vector<int> orderedObjs;
    {
        set<int> pageSet(pageIds.begin(), pageIds.end());
        for (auto &kv : combined) {
            if (!pageSet.count(kv.first)) orderedObjs.push_back(kv.first);
        }
        for (int pid : pageIds) orderedObjs.push_back(pid);
    }

    if (!writeMergedPDF(out, combined, orderedObjs, pageIds)) {
        cerr << (isChinese ? "写入合并后的 PDF 失败。" : "Failed to write merged PDF.") << "\n";
        return 5;
    }

    cout << successMsg << out << "\n";
    return 0;
}
