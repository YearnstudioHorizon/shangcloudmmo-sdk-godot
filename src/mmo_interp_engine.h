#ifndef MMO_INTERP_ENGINE_H
#define MMO_INTERP_ENGINE_H

#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/hash_set.hpp>
#include <godot_cpp/templates/pair.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/dictionary.hpp>

namespace godot {

// 插帧同步引擎（移植自 core.js 的 _ensureInterpLoop / _mmoInterpState）
//
// 接收端按 uid / varName 维护 { current, target } 状态，在 _process 中逐帧用
// 帧率无关的指数平滑把 current 逼近 target。规则与 core.js 完全一致：
//   - 首次收到：snap
//   - 瞬移阈值（|target - current| >= 200）：snap
//   - 正常：只更新 target，由 tick 做平滑写入 current
//   - 动态追赶：|diff| > 50 时提高因子（上限 0.8）
//   - 自动休眠：所有变量均到达 target 时 tick 直接返回空
//
// 非数值、或不在 interp 集合中的变量，不参与插帧，仅存原始值供 get_sync_var 回退。
class MmoInterpEngine {
public:
	struct InterpState {
		double current = 0.0;
		double target = 0.0;
	};

	struct Change {
		String uid;
		String var_name;
		double value = 0.0;
	};

	MmoInterpEngine();

	// 应用一次 __sync_var__ 更新。vars: varName -> 值（字符串）；interp: 需要 interp 的 varName 集合（可为空，沿用已有集合）
	void apply_sync(const String &p_uid, const Dictionary &p_vars, const PackedStringArray &p_interp);

	// 在 _process 中调用，推进所有插帧变量 current → target。返回本次发生显著变化的列表。
	TypedArray<Dictionary> tick(double p_delta_seconds);

	// 读取插帧变量当前值（平滑后的 current）；不在插帧集合或尚未建立状态时回退到最近原始值
	double get_sync_var(const String &p_uid, const String &p_name) const;
	// 读取原始字符串值（不做插帧）
	String get_sync_var_raw(const String &p_uid, const String &p_name) const;

	// 清理指定 uid 的状态（玩家离开时调用）
	void clear_uid(const String &p_uid);
	// 清空全部状态（断开连接时调用）
	void clear();

private:
	static constexpr double BASE_FACTOR = 0.15;          // _MMO_INTERP_BASE_FACTOR
	static constexpr double TELEPORT_THRESHOLD = 200.0;  // _MMO_INTERP_TELEPORT_THRESHOLD
	static constexpr double EPSILON = 0.001;
	static constexpr double FRAME_REF_MS = 16.67;

	// uid -> (varName -> state)
	HashMap<String, HashMap<String, InterpState>> state;
	// uid -> 需要 interp 的 varName 集合
	HashMap<String, HashSet<String>> interp_names;
	// uid -> (varName -> 最近原始值)
	HashMap<String, HashMap<String, String>> raw;

	bool is_numeric(const String &p_str, double &r_out) const;
};

} // namespace godot

#endif // MMO_INTERP_ENGINE_H
