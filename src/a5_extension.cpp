#define DUCKDB_EXTENSION_MAIN

#include "a5_extension.hpp"
#include "duckdb.hpp"
#include "duckdb/common/exception.hpp"
#include "duckdb/function/scalar_function.hpp"
#include <duckdb/parser/parsed_data/create_scalar_function_info.hpp>

namespace duckdb {

inline void A5ScalarFun(DataChunk &args, ExpressionState &state, Vector &result) {
	auto &name_vector = args.data[0];
	UnaryExecutor::Execute<string_t, string_t>(name_vector, result, args.size(), [&](string_t name) {
		return StringVector::AddString(result, "A5 " + name.GetString() + " 🐥");
	});
}

static void LoadInternal(ExtensionLoader &loader) {
	// Register a scalar function
	auto a5_scalar_function = ScalarFunction("a5", {LogicalType::VARCHAR}, LogicalType::VARCHAR, A5ScalarFun);
	loader.RegisterFunction(a5_scalar_function);
}

void A5Extension::Load(ExtensionLoader &loader) {
	LoadInternal(loader);
}
std::string A5Extension::Name() {
	return "a5";
}

std::string A5Extension::Version() const {
#ifdef EXT_VERSION_QUACK
	return EXT_VERSION_QUACK;
#else
	return "";
#endif
}

} // namespace duckdb

extern "C" {

DUCKDB_CPP_EXTENSION_ENTRY(a5, loader) {
	duckdb::LoadInternal(loader);
}
}
