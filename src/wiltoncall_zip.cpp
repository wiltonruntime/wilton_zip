/* 
 * File:   wiltoncall_zip.cpp
 * Author: alex
 *
 * Created on December 3, 2017, 7:17 PM
 */

#include <functional>
#include <string>
#include <vector>

#include "staticlib/compress.hpp"
#include "staticlib/json.hpp"
#include "staticlib/support.hpp"
#include "staticlib/tinydir.hpp"
#include "staticlib/unzip.hpp"
#include "staticlib/utils.hpp"

#include "wilton/support/buffer.hpp"
#include "wilton/support/exception.hpp"
#include "wilton/support/registrar.hpp"

namespace wilton {
namespace zip {

support::buffer read_file(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rpath = std::ref(sl::utils::empty_string());
    auto hex = false;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("path" == name) {
            rpath = fi.as_string_nonempty_or_throw(name);
        } else if ("hex" == name) {
            hex = fi.as_bool_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rpath.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'path' not specified"));
    const std::string& path = rpath.get();
    // read file
    auto idx = sl::unzip::file_index(path);
    auto res = std::vector<sl::json::field>();
    for (auto& en : idx.get_entries()) {
        auto stream = sl::unzip::open_zip_entry(idx, en);
        auto src = sl::io::streambuf_source(stream.get());
        auto sink = sl::io::string_sink();
        if (hex) {
            sl::io::copy_to_hex(src, sink);
        } else {
            sl::io::copy_all(src, sink);
        }
        auto fi = sl::json::field(en, std::move(sink.get_string()));
        res.emplace_back(std::move(fi));
    }
    return support::make_json_buffer(std::move(res));
}

support::buffer read_file_entry(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rpath = std::ref(sl::utils::empty_string());
    auto rentry = std::ref(sl::utils::empty_string());
    auto hex = false;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("path" == name) {
            rpath = fi.as_string_nonempty_or_throw(name);
        } else if ("entry" == name) {
            rentry = fi.as_string_nonempty_or_throw(name);
        } else if ("hex" == name) {
            hex = fi.as_bool_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rpath.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'path' not specified"));
    if (rentry.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'entry' not specified"));
    const std::string& path = rpath.get();
    const std::string& entry = rentry.get();
    // read file
    auto idx = sl::unzip::file_index(path);
    sl::unzip::file_entry en = idx.find_zip_entry(entry);
    if (en.is_empty()) throw support::exception(TRACEMSG(
            "Invalid ZIP entry specified: [" + entry + "], file: [" + path + "]"));
    auto stream = sl::unzip::open_zip_entry(idx, entry);
    auto src = sl::io::streambuf_source(stream.get());
    if (hex) {
        return support::make_hex_buffer(src);
    } else {
        return support::make_source_buffer(src);
    }
}

support::buffer list_file_entries(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rpath = std::ref(sl::utils::empty_string());
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("path" == name) {
            rpath = fi.as_string_nonempty_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rpath.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'path' not specified"));
    const std::string& path = rpath.get();
    // read file
    auto idx = sl::unzip::file_index(path);
    auto res = std::vector<sl::json::value>();
    for (auto& en : idx.get_entries()) {
        res.push_back(en);
    }
    return support::make_json_buffer(std::move(res));
}

support::buffer write_file(sl::io::span<const char> data) {
    // json parse
    auto json = sl::json::load(data);
    auto rpath = std::ref(sl::utils::empty_string());
    auto rentries = std::ref(sl::json::null_value_ref());
    auto hex = false;
    for (const sl::json::field& fi : json.as_object()) {
        auto& name = fi.name();
        if ("path" == name) {
            rpath = fi.as_string_nonempty_or_throw(name);
        } else if ("entries" == name) {
            rentries = fi.val();
        } else if ("hex" == name) {
            hex = fi.as_bool_or_throw(name);
        } else {
            throw support::exception(TRACEMSG("Unknown data field: [" + name + "]"));
        }
    }
    if (rpath.get().empty()) throw support::exception(TRACEMSG(
            "Required parameter 'path' not specified"));
    if (sl::json::type::nullt == rentries.get().json_type()) throw support::exception(TRACEMSG(
            "Required parameter 'entries' not specified"));
    if (sl::json::type::object != rentries.get().json_type()) throw support::exception(TRACEMSG(
            "Parameter 'entries' must be an 'object'," +
            " specified type: [" + sl::json::stringify_json_type(rentries.get().json_type()) + "]"));
    const std::string& path = rpath.get();
    const std::vector<sl::json::field>& entries = rentries.get().as_object();
    // write file
    auto sink = sl::compress::make_zip_sink(sl::tinydir::file_sink(path));
    for (const sl::json::field& en : entries) {
        if (sl::json::type::string != en.json_type()) throw support::exception(TRACEMSG(
                "Parameter 'entries' must be an 'object' with 'string' field values," +
                " specified type: [" + sl::json::stringify_json_type(en.json_type()) + "]," +
                " entry: [" + en.name() + "]"));
        sink.add_entry(en.name());
        auto src = sl::io::string_source(en.val().as_string());
        if (hex) {
            sl::io::copy_from_hex(src, sink);
        } else {
            sl::io::copy_all(src, sink);
        }
    }
    return support::make_empty_buffer();
}

} // namespace
}

extern "C" char* wilton_module_init() {
    try {
        wilton::support::register_wiltoncall("zip_read_file", wilton::zip::read_file);
        wilton::support::register_wiltoncall("zip_read_file_entry", wilton::zip::read_file_entry);
        wilton::support::register_wiltoncall("zip_list_file_entries", wilton::zip::list_file_entries);
        wilton::support::register_wiltoncall("zip_write_file", wilton::zip::write_file);
        return nullptr;
    } catch (const std::exception& e) {
        return wilton::support::alloc_copy(TRACEMSG(e.what() + "\nException raised"));
    }
}
