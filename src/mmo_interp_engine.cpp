#include "mmo_interp_engine.h"

#include <godot_cpp/variant/utility_functions.hpp>

namespace godot {

MmoInterpEngine::MmoInterpEngine() {
}

bool MmoInterpEngine::is_numeric(const String &p_str, double &r_out) const {
	String s = p_str.strip_edges();
	if (s.is_empty()) return false;
	if (!UtilityFunctions::is_numeric(s)) {
		return false;
	}
	r_out = s.to_float();
	return true;
}

void MmoInterpEngine::apply_sync(const String &p_uid, const Dictionary &p_vars, const PackedStringArray &p_interp) {
	if (p_uid.is_empty() || p_vars.is_empty()) return;

	if (!state.has(p_uid)) {
		state.insert(p_uid, HashMap<String, InterpState>());
	}
	if (!raw.has(p_uid)) {
		raw.insert(p_uid, HashMap<String, String>());
	}
	HashMap<String, InterpState> &by_var = state[p_uid];
	HashMap<String, String> &raw_by_var = raw[p_uid];

	// 更新 interp 集合
	if (p_interp.size() > 0) {
		HashSet<String> interp_set;
		for (int i = 0; i < p_interp.size(); i++) {
			interp_set.insert(p_interp[i]);
		}
		interp_names[p_uid] = interp_set;
	}

	const HashSet<String> *interp_set_ptr = interp_names.getptr(p_uid);

	// 遍历 vars
	Array keys = p_vars.keys();
	for (int i = 0; i < keys.size(); i++) {
		const String &name = keys[i];
		String v = String(p_vars[name]);
		raw_by_var[name] = v;

		bool need_interp = interp_set_ptr && interp_set_ptr->has(name);
		if (!need_interp) continue;

		double num_v = 0.0;
		if (!is_numeric(v, num_v)) {
			continue; // 非数值不插帧
		}

		InterpState *st_ptr = by_var.getptr(name);
		if (!st_ptr) {
			// 首次收到：snap
			by_var.insert(name, InterpState{ num_v, num_v });
		} else if (Math::abs(num_v - st_ptr->current) >= TELEPORT_THRESHOLD) {
			// 瞬移检测：snap
			*st_ptr = InterpState{ num_v, num_v };
		} else {
			// 正常：仅更新 target
			st_ptr->target = num_v;
		}
	}
}

TypedArray<Dictionary> MmoInterpEngine::tick(double p_delta_seconds) {
	TypedArray<Dictionary> changed;
	if (state.is_empty()) {
		return changed;
	}

	double dt_ms = p_delta_seconds * 1000.0;
	// 帧率无关平滑因子
	double base_factor = 1.0 - Math::pow(1.0 - BASE_FACTOR, dt_ms / FRAME_REF_MS);

	for (auto &uid_entry : state) {
		const String &uid = uid_entry.key;
		HashMap<String, InterpState> &by_var = uid_entry.value;
		for (auto &name_entry : by_var) {
			const String &name = name_entry.key;
			InterpState &st = name_entry.value;
			double diff = st.target - st.current;

			if (Math::abs(diff) <= EPSILON) {
				st.current = st.target;
				continue;
			}

			// 动态追赶：偏差过大时提高因子
			double factor = base_factor;
			if (Math::abs(diff) > 50.0) {
				factor = MIN(base_factor * (Math::abs(diff) / 50.0), 0.8);
			}

			st.current += diff * factor;

			// 脏检查：仅在变化显著时上报
			if (Math::abs(diff * factor) > EPSILON) {
				Dictionary c;
				c["uid"] = uid;
				c["var_name"] = name;
				c["value"] = st.current;
				changed.push_back(c);
			}
		}
	}
	return changed;
}

double MmoInterpEngine::get_sync_var(const String &p_uid, const String &p_name) const {
	const HashMap<String, InterpState> *by_var_ptr = state.getptr(p_uid);
	if (by_var_ptr) {
		const InterpState *st_ptr = by_var_ptr->getptr(p_name);
		if (st_ptr) {
			return st_ptr->current;
		}
	}
	const HashMap<String, String> *raw_by_var_ptr = raw.getptr(p_uid);
	if (raw_by_var_ptr) {
		const String *raw_ptr = raw_by_var_ptr->getptr(p_name);
		if (raw_ptr) {
			double d = 0.0;
			if (is_numeric(*raw_ptr, d)) {
				return d;
			}
		}
	}
	return 0.0;
}

String MmoInterpEngine::get_sync_var_raw(const String &p_uid, const String &p_name) const {
	const HashMap<String, String> *raw_by_var_ptr = raw.getptr(p_uid);
	if (raw_by_var_ptr) {
		const String *raw_ptr = raw_by_var_ptr->getptr(p_name);
		if (raw_ptr) {
			return *raw_ptr;
		}
	}
	return String();
}

void MmoInterpEngine::clear_uid(const String &p_uid) {
	if (p_uid.is_empty()) return;
	state.erase(p_uid);
	interp_names.erase(p_uid);
	raw.erase(p_uid);
}

void MmoInterpEngine::clear() {
	state.clear();
	interp_names.clear();
	raw.clear();
}

} // namespace godot
