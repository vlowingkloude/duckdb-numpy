#include "capi_tester.hpp"

using namespace duckdb;
using namespace std;

TEST_CASE("Test table_info incorrect 'is_valid' value for 'dflt_value' column", "[capi]") {
	duckdb_database db;
	duckdb_connection con;
	duckdb_result result;

	REQUIRE(duckdb_open(NULL, &db) != DuckDBError);
	REQUIRE(duckdb_connect(db, &con) != DuckDBError);
	//! Create a table with 40 columns
	REQUIRE(duckdb_query(con,
	                     "CREATE TABLE foo (c00 varchar, c01 varchar, c02 varchar, c03 varchar, c04 varchar, c05 "
	                     "varchar, c06 varchar, c07 varchar, c08 varchar, c09 varchar, c10 varchar, c11 varchar, c12 "
	                     "varchar, c13 varchar, c14 varchar, c15 varchar, c16 varchar, c17 varchar, c18 varchar, c19 "
	                     "varchar, c20 varchar, c21 varchar, c22 varchar, c23 varchar, c24 varchar, c25 varchar, c26 "
	                     "varchar, c27 varchar, c28 varchar, c29 varchar, c30 varchar, c31 varchar, c32 varchar, c33 "
	                     "varchar, c34 varchar, c35 varchar, c36 varchar, c37 varchar, c38 varchar, c39 varchar);",
	                     NULL) != DuckDBError);
	//! Get table info for the created table
	REQUIRE(duckdb_query(con, "PRAGMA table_info(foo);", &result) != DuckDBError);

	//! Columns ({cid, name, type, notnull, dflt_value, pk}}
	idx_t col_count = duckdb_column_count(&result);
	REQUIRE(col_count == 6);
	idx_t chunk_count = duckdb_result_chunk_count(result);

	// Loop over the produced chunks
	for (idx_t chunk_idx = 0; chunk_idx < chunk_count; chunk_idx++) {
		duckdb_data_chunk chunk = duckdb_result_get_chunk(result, chunk_idx);
		idx_t row_count = duckdb_data_chunk_get_size(chunk);

		for (idx_t row_idx = 0; row_idx < row_count; row_idx++) {
			for (idx_t col_idx = 0; col_idx < col_count; col_idx++) {
				//! Get the column
				duckdb_vector vector = duckdb_data_chunk_get_vector(chunk, col_idx);
				uint64_t *validity = duckdb_vector_get_validity(vector);
				bool is_valid = duckdb_validity_row_is_valid(validity, row_idx);

				if (col_idx == 4) {
					//'dflt_value' column
					REQUIRE(is_valid == false);
				}
			}
		}
		duckdb_destroy_data_chunk(&chunk);
	}

	duckdb_destroy_result(&result);
	duckdb_disconnect(&con);
	duckdb_close(&db);
}

TEST_CASE("Test Logical Types C API", "[capi]") {
	duckdb_logical_type type = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	REQUIRE(type);
	REQUIRE(duckdb_get_type_id(type) == DUCKDB_TYPE_BIGINT);
	duckdb_destroy_logical_type(&type);
	duckdb_destroy_logical_type(&type);

	// list type
	duckdb_logical_type elem_type = duckdb_create_logical_type(DUCKDB_TYPE_INTEGER);
	duckdb_logical_type list_type = duckdb_create_list_type(elem_type);
	REQUIRE(duckdb_get_type_id(list_type) == DUCKDB_TYPE_LIST);
	duckdb_logical_type elem_type_dup = duckdb_list_type_child_type(list_type);
	REQUIRE(elem_type_dup != elem_type);
	REQUIRE(duckdb_get_type_id(elem_type_dup) == duckdb_get_type_id(elem_type));
	duckdb_destroy_logical_type(&elem_type);
	duckdb_destroy_logical_type(&list_type);
	duckdb_destroy_logical_type(&elem_type_dup);

	// map type
	duckdb_logical_type key_type = duckdb_create_logical_type(DUCKDB_TYPE_SMALLINT);
	duckdb_logical_type value_type = duckdb_create_logical_type(DUCKDB_TYPE_DOUBLE);
	duckdb_logical_type map_type = duckdb_create_map_type(key_type, value_type);
	REQUIRE(duckdb_get_type_id(map_type) == DUCKDB_TYPE_MAP);
	duckdb_logical_type key_type_dup = duckdb_map_type_key_type(map_type);
	duckdb_logical_type value_type_dup = duckdb_map_type_value_type(map_type);
	REQUIRE(key_type_dup != key_type);
	REQUIRE(value_type_dup != value_type);
	REQUIRE(duckdb_get_type_id(key_type_dup) == duckdb_get_type_id(key_type));
	REQUIRE(duckdb_get_type_id(value_type_dup) == duckdb_get_type_id(value_type));
	duckdb_destroy_logical_type(&key_type);
	duckdb_destroy_logical_type(&value_type);
	duckdb_destroy_logical_type(&map_type);
	duckdb_destroy_logical_type(&key_type_dup);
	duckdb_destroy_logical_type(&value_type_dup);

	duckdb_destroy_logical_type(nullptr);
}

TEST_CASE("Test DataChunk C API", "[capi]") {
	CAPITester tester;
	duckdb::unique_ptr<CAPIResult> result;
	duckdb_state status;

	REQUIRE(tester.OpenDatabase(nullptr));

	REQUIRE(duckdb_vector_size() == STANDARD_VECTOR_SIZE);

	tester.Query("CREATE TABLE test(i BIGINT, j SMALLINT)");

	duckdb_logical_type types[2];
	types[0] = duckdb_create_logical_type(DUCKDB_TYPE_BIGINT);
	types[1] = duckdb_create_logical_type(DUCKDB_TYPE_SMALLINT);

	auto data_chunk = duckdb_create_data_chunk(types, 2);
	REQUIRE(data_chunk);

	REQUIRE(duckdb_data_chunk_get_column_count(data_chunk) == 2);

	auto first_type = duckdb_vector_get_column_type(duckdb_data_chunk_get_vector(data_chunk, 0));
	REQUIRE(duckdb_get_type_id(first_type) == DUCKDB_TYPE_BIGINT);
	duckdb_destroy_logical_type(&first_type);

	auto second_type = duckdb_vector_get_column_type(duckdb_data_chunk_get_vector(data_chunk, 1));
	REQUIRE(duckdb_get_type_id(second_type) == DUCKDB_TYPE_SMALLINT);
	duckdb_destroy_logical_type(&second_type);

	REQUIRE(duckdb_data_chunk_get_vector(data_chunk, 999) == nullptr);
	REQUIRE(duckdb_data_chunk_get_vector(nullptr, 0) == nullptr);
	REQUIRE(duckdb_vector_get_column_type(nullptr) == nullptr);

	REQUIRE(duckdb_data_chunk_get_size(data_chunk) == 0);
	REQUIRE(duckdb_data_chunk_get_size(nullptr) == 0);

	// use the appender to insert a value using the data chunk API

	duckdb_appender appender;
	status = duckdb_appender_create(tester.connection, nullptr, "test", &appender);
	REQUIRE(status == DuckDBSuccess);

	// append standard primitive values
	auto col1_ptr = (int64_t *)duckdb_vector_get_data(duckdb_data_chunk_get_vector(data_chunk, 0));
	*col1_ptr = 42;
	auto col2_ptr = (int16_t *)duckdb_vector_get_data(duckdb_data_chunk_get_vector(data_chunk, 1));
	*col2_ptr = 84;

	REQUIRE(duckdb_vector_get_data(nullptr) == nullptr);

	duckdb_data_chunk_set_size(data_chunk, 1);
	REQUIRE(duckdb_data_chunk_get_size(data_chunk) == 1);

	REQUIRE(duckdb_append_data_chunk(appender, data_chunk) == DuckDBSuccess);
	REQUIRE(duckdb_append_data_chunk(appender, nullptr) == DuckDBError);
	REQUIRE(duckdb_append_data_chunk(nullptr, data_chunk) == DuckDBError);

	// append nulls
	duckdb_data_chunk_reset(data_chunk);
	REQUIRE(duckdb_data_chunk_get_size(data_chunk) == 0);

	duckdb_vector_ensure_validity_writable(duckdb_data_chunk_get_vector(data_chunk, 0));
	duckdb_vector_ensure_validity_writable(duckdb_data_chunk_get_vector(data_chunk, 1));
	auto col1_validity = duckdb_vector_get_validity(duckdb_data_chunk_get_vector(data_chunk, 0));
	REQUIRE(duckdb_validity_row_is_valid(col1_validity, 0));
	duckdb_validity_set_row_validity(col1_validity, 0, false);
	REQUIRE(!duckdb_validity_row_is_valid(col1_validity, 0));

	auto col2_validity = duckdb_vector_get_validity(duckdb_data_chunk_get_vector(data_chunk, 1));
	REQUIRE(col2_validity);
	REQUIRE(duckdb_validity_row_is_valid(col2_validity, 0));
	duckdb_validity_set_row_validity(col2_validity, 0, false);
	REQUIRE(!duckdb_validity_row_is_valid(col2_validity, 0));

	duckdb_data_chunk_set_size(data_chunk, 1);
	REQUIRE(duckdb_data_chunk_get_size(data_chunk) == 1);

	REQUIRE(duckdb_append_data_chunk(appender, data_chunk) == DuckDBSuccess);

	REQUIRE(duckdb_vector_get_validity(nullptr) == nullptr);

	duckdb_appender_destroy(&appender);

	result = tester.Query("SELECT * FROM test");
	REQUIRE_NO_FAIL(*result);
	REQUIRE(result->Fetch<int64_t>(0, 0) == 42);
	REQUIRE(result->Fetch<int16_t>(1, 0) == 84);
	REQUIRE(result->IsNull(0, 1));
	REQUIRE(result->IsNull(1, 1));

	duckdb_data_chunk_reset(data_chunk);
	duckdb_data_chunk_reset(nullptr);
	REQUIRE(duckdb_data_chunk_get_size(data_chunk) == 0);

	duckdb_destroy_data_chunk(&data_chunk);
	duckdb_destroy_data_chunk(&data_chunk);

	duckdb_destroy_data_chunk(nullptr);

	duckdb_destroy_logical_type(&types[0]);
	duckdb_destroy_logical_type(&types[1]);
}

TEST_CASE("Test DataChunk result fetch in C API", "[capi]") {
	CAPITester tester;
	duckdb::unique_ptr<CAPIResult> result;

	if (duckdb_vector_size() < 64) {
		return;
	}

	REQUIRE(tester.OpenDatabase(nullptr));

	// fetch a small result set
	result = tester.Query("SELECT CASE WHEN i=1 THEN NULL ELSE i::INTEGER END i FROM range(3) tbl(i)");
	REQUIRE(NO_FAIL(*result));
	REQUIRE(result->ColumnCount() == 1);
	REQUIRE(result->row_count() == 3);
	REQUIRE(result->ErrorMessage() == nullptr);

	// fetch the first chunk
	REQUIRE(result->ChunkCount() == 1);
	auto chunk = result->FetchChunk(0);
	REQUIRE(chunk);

	REQUIRE(chunk->ColumnCount() == 1);
	REQUIRE(chunk->size() == 3);

	auto data = (int32_t *)chunk->GetData(0);
	auto validity = chunk->GetValidity(0);
	REQUIRE(data[0] == 0);
	REQUIRE(data[2] == 2);
	REQUIRE(duckdb_validity_row_is_valid(validity, 0));
	REQUIRE(!duckdb_validity_row_is_valid(validity, 1));
	REQUIRE(duckdb_validity_row_is_valid(validity, 2));

	// after fetching a chunk, we cannot use the old API anymore
	REQUIRE(result->ColumnData<int32_t>(0) == nullptr);
	REQUIRE(result->Fetch<int32_t>(0, 1) == 0);

	// result set is exhausted!
	chunk = result->FetchChunk(1);
	REQUIRE(!chunk);
}

TEST_CASE("Test DataChunk populate ListVector in C API", "[capi]") {
	REQUIRE(duckdb_list_vector_reserve(nullptr, 100) == duckdb_state::DuckDBError);
	REQUIRE(duckdb_list_vector_set_size(nullptr, 200) == duckdb_state::DuckDBError);

	auto elem_type = duckdb_create_logical_type(duckdb_type::DUCKDB_TYPE_INTEGER);
	auto list_type = duckdb_create_list_type(elem_type);
	duckdb_logical_type schema[] = {list_type};
	auto chunk = duckdb_create_data_chunk(schema, 1);
	auto list_vector = duckdb_data_chunk_get_vector(chunk, 0);

	REQUIRE(duckdb_list_vector_reserve(list_vector, 123) == duckdb_state::DuckDBSuccess);
	REQUIRE(duckdb_list_vector_get_size(list_vector) == 0);
	auto child = duckdb_list_vector_get_child(list_vector);
	for (int i = 0; i < 123; i++) {
		((int *)duckdb_vector_get_data(child))[i] = i;
	}
	REQUIRE(duckdb_list_vector_set_size(list_vector, 123) == duckdb_state::DuckDBSuccess);
	REQUIRE(duckdb_list_vector_get_size(list_vector) == 123);

	auto entries = (duckdb_list_entry *)duckdb_vector_get_data(list_vector);
	entries[0].offset = 0;
	entries[0].length = 20;
	entries[1].offset = 20;
	entries[1].offset = 80;
	entries[2].offset = 100;
	entries[2].length = 23;

	auto vector = (Vector &)(*list_vector);
	for (int i = 0; i < 123; i++) {
		REQUIRE(ListVector::GetEntry(vector).GetValue(i) == i);
	}

	duckdb_destroy_data_chunk(&chunk);
	duckdb_destroy_logical_type(&list_type);
	duckdb_destroy_logical_type(&elem_type);
}
