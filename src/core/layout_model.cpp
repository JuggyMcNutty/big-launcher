#include <memory>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <system_error>

#include <libxml/xmlmemory.h>
#include <libxml/parser.h>
#include <libxml/tree.h>

#include "layout_model.hpp"
#include "../render/logger.hpp"

namespace BL {
    void free_xml_char(xmlChar *c) { xmlFree((void*) c); }
    using pXmlChar = std::unique_ptr<xmlChar, decltype(&free_xml_char)>;
    using pXmlDoc  = std::unique_ptr<xmlDoc,  decltype(&xmlFreeDoc)>;

    static std::string attr(xmlNodePtr node, const char *name)
    {
        pXmlChar v(xmlGetProp(node, (const xmlChar*) name), &free_xml_char);
        return v ? std::string((const char*) v.get()) : std::string();
    }
    static std::string content(xmlNodePtr node)
    {
        pXmlChar v(xmlNodeGetContent(node), &free_xml_char);
        return v ? std::string((const char*) v.get()) : std::string();
    }
}

void BL::LayoutModel::parse(const std::string &file)
{
    BL::logger::debug("Parsing layout file '{}'", file);
    pXmlDoc doc(xmlParseFile(file.c_str()), &xmlFreeDoc);
    if (!doc)
        BL::logger::critical("Failed to parse layout file '{}'", file);
    xmlNodePtr root = xmlDocGetRootElement(doc.get());
    if (!root || xmlStrcmp(root->name, (const xmlChar*) "layout"))
        BL::logger::critical("Layout file root element is not <layout>");

    menus.clear();
    sidebar_commands.clear();

    for (xmlNodePtr node = root->xmlChildrenNode; node; node = node->next) {
        if (!xmlStrcmp(node->name, (const xmlChar*) "menu")) {
            Menu menu;
            menu.title = attr(node, "title");
            for (xmlNodePtr e = node->xmlChildrenNode; e; e = e->next) {
                if (xmlStrcmp(e->name, (const xmlChar*) "entry")) continue;
                Entry entry;
                entry.title = attr(e, "title");
                for (xmlNodePtr c = e->xmlChildrenNode; c; c = c->next) {
                    if (!xmlStrcmp(c->name, (const xmlChar*) "command"))
                        entry.command = content(c);
                    else if (!xmlStrcmp(c->name, (const xmlChar*) "card")) {
                        std::string txt = content(c);
                        if (txt.empty() || xmlChildElementCount(c)) {
                            for (xmlNodePtr cc = c->xmlChildrenNode; cc; cc = cc->next) {
                                if (!xmlStrcmp(cc->name, (const xmlChar*) "icon")) {
                                    entry.card.icon = content(cc);
                                    pXmlChar m(xmlGetProp(cc, (const xmlChar*) "margin"), &free_xml_char);
                                    if (m) {
                                        std::string ms = (const char*) m.get();
                                        if (!ms.empty() && ms.back() == '%') ms.pop_back();
                                        entry.card.icon_margin = atoi(ms.c_str());
                                    }
                                }
                                else if (!xmlStrcmp(cc->name, (const xmlChar*) "background"))
                                    entry.card.background = content(cc);
                            }
                        }
                        else
                            entry.card.custom = txt;
                    }
                }
                menu.entries.push_back(std::move(entry));
            }
            if (!menu.entries.empty())
                menus.push_back(std::move(menu));
        }
        else if (!xmlStrcmp(node->name, (const xmlChar*) "command")) {
            SidebarCommand sc;
            sc.title = attr(node, "title");
            sc.command = content(node);
            if (!sc.title.empty())
                sidebar_commands.push_back(std::move(sc));
        }
    }
    BL::logger::debug("Parsed {} menus, {} sidebar commands",
                      menus.size(), sidebar_commands.size());
}

void BL::LayoutModel::write(const std::string &file) const
{
    pXmlDoc doc(xmlNewDoc((const xmlChar*) "1.0"), &xmlFreeDoc);
    xmlNodePtr root = xmlNewNode(nullptr, (const xmlChar*) "layout");
    xmlDocSetRootElement(doc.get(), root);

    for (const Menu &menu : menus) {
        xmlNodePtr m = xmlNewNode(nullptr, (const xmlChar*) "menu");
        xmlNewProp(m, (const xmlChar*) "title", (const xmlChar*) menu.title.c_str());
        for (const Entry &e : menu.entries) {
            xmlNodePtr en = xmlNewNode(nullptr, (const xmlChar*) "entry");
            xmlNewProp(en, (const xmlChar*) "title", (const xmlChar*) e.title.c_str());

            xmlNodePtr cmd = xmlNewNode(nullptr, (const xmlChar*) "command");
            xmlNodeAddContent(cmd, (const xmlChar*) e.command.c_str());
            xmlAddChild(en, cmd);

            xmlNodePtr card = xmlNewNode(nullptr, (const xmlChar*) "card");
            if (!e.card.custom.empty()) {
                xmlNodeAddContent(card, (const xmlChar*) e.card.custom.c_str());
            }
            else {
                xmlNodePtr icon = xmlNewNode(nullptr, (const xmlChar*) "icon");
                if (e.card.icon_margin != 12)
                    xmlNewProp(icon, (const xmlChar*) "margin",
                               (const xmlChar*) (std::to_string(e.card.icon_margin) + "%").c_str());
                xmlNodeAddContent(icon, (const xmlChar*) e.card.icon.c_str());
                xmlAddChild(card, icon);
                if (!e.card.background.empty()) {
                    xmlNodePtr bg = xmlNewNode(nullptr, (const xmlChar*) "background");
                    xmlNodeAddContent(bg, (const xmlChar*) e.card.background.c_str());
                    xmlAddChild(card, bg);
                }
            }
            xmlAddChild(en, card);
            xmlAddChild(m, en);
        }
        xmlAddChild(root, m);
    }

    for (const SidebarCommand &sc : sidebar_commands) {
        xmlNodePtr c = xmlNewNode(nullptr, (const xmlChar*) "command");
        xmlNewProp(c, (const xmlChar*) "title", (const xmlChar*) sc.title.c_str());
        xmlNodeAddContent(c, (const xmlChar*) sc.command.c_str());
        xmlAddChild(root, c);
    }

    std::error_code ec;
    std::filesystem::path bak(file); bak += ".bak";
    if (std::filesystem::exists(file, ec))
        std::filesystem::copy_file(file, bak, std::filesystem::copy_options::overwrite_existing, ec);
    std::filesystem::path tmp(file); tmp += ".tmp";
    if (xmlSaveFormatFileEnc((const char*) tmp.string().c_str(), doc.get(), "UTF-8", 1) < 0)
        BL::logger::error("Failed to write layout file '{}'", file);
    std::filesystem::rename(tmp, file, ec);
    if (ec)
        BL::logger::error("Failed to rename layout file: {}", ec.message());
    BL::logger::debug("Wrote layout file '{}'", file);
}

BL::Menu* BL::LayoutModel::find_menu(const std::string &title)
{
    for (Menu &m : menus)
        if (m.title == title) return &m;
    return nullptr;
}
