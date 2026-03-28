/*
 *  yosys -- Yosys Open SYnthesis Suite
 *
 *  Copyright (C) 2024  ChipFlow
 *
 *  Permission to use, copy, modify, and/or distribute this software for any
 *  purpose with or without fee is hereby granted, provided that the above
 *  copyright notice and this permission notice appear in all copies.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 *  WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 *  MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 *  ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 *  WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 *  ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 *  OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 *
 *  ---
 *
 *  Shared utilities for parsing the XAIGER "y" extension origin mapping.
 *  Used by both frontends/aiger/aigerparse.cc and frontends/aiger2/xaiger.cc.
 */

#ifndef AIGER_ORIGINS_H
#define AIGER_ORIGINS_H

#include "kernel/yosys.h"

YOSYS_NAMESPACE_BEGIN

// Sentinel value written by ABC at position 0 of the "y" section to
// distinguish the new variable-length multi-origin format from the old
// flat single-origin format.
static constexpr int32_t ORIGINS_FORMAT_SENTINEL = -2;

// Parse the "y" extension payload into a map from AIG object ID to
// a list of origin literals.
//
// The payload is native-endian int32 data (written by ABC via fwrite
// on the same host). Two formats are supported:
//
//   Old (single-origin): flat array, one literal per AIG object.
//   New (multi-origin):  sentinel -2, then per-object [count, lit0, lit1, ...]
//                        in dense AIG object order (every object has an entry,
//                        including those with count=0).
inline dict<int, std::vector<int32_t>>
parse_origin_lits(const std::vector<int32_t> &raw)
{
	dict<int, std::vector<int32_t>> origin_lits;
	uint32_t n = raw.size();
	if (n >= 1 && raw[0] == ORIGINS_FORMAT_SENTINEL) {
		// New multi-origin format
		uint32_t pos = 1;
		for (int obj_id = 0; pos < n; obj_id++) {
			int32_t count = raw[pos++];
			if (count < 0)
				log_error("Corrupt 'y' extension: negative count %d at obj %d\n", count, obj_id);
			std::vector<int32_t> lits;
			for (int32_t j = 0; j < count && pos < n; j++, pos++)
				if (raw[pos] >= 0)
					lits.push_back(raw[pos]);
			if (!lits.empty())
				origin_lits[obj_id] = std::move(lits);
		}
		log_debug("y: parsed multi-origin format, %zu objects with origins\n", origin_lits.size());
	} else {
		// Old single-origin format: one literal per AIG object
		origin_lits.reserve(n);
		for (uint32_t i = 0; i < n; i++)
			if (raw[i] >= 0)
				origin_lits[i] = {raw[i]};
		log_debug("y: parsed single-origin format, %u entries\n", n);
	}
	return origin_lits;
}

// Collect \src strings for all origins of an AIG object.
// Looks up each origin literal in obj_src (from the .sym map file)
// and inserts any found \src strings into src_values.
inline void collect_origin_src(
	int obj_id,
	const dict<int, std::vector<int32_t>> &origin_lits,
	const dict<int, std::string> &obj_src,
	pool<std::string> &src_values)
{
	auto orig_it = origin_lits.find(obj_id);
	if (orig_it != origin_lits.end()) {
		for (int32_t lit : orig_it->second) {
			auto src_it = obj_src.find(lit >> 1);
			if (src_it != obj_src.end())
				src_values.insert(src_it->second);
		}
	}
}

YOSYS_NAMESPACE_END

#endif // AIGER_ORIGINS_H
