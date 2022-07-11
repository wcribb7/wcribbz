#include "clar_libgit2.h"
#include "git2/odb_backend.h"

#include "pack_data_one_sha256.h"
#include "pack.h"

#ifdef GIT_EXPERIMENTAL_SHA256
static git_odb *_odb;
#endif

void test_odb_packed_one_sha256__initialize(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	git_odb_backend *backend = NULL;
	git_odb_options odb_opts = GIT_ODB_OPTIONS_INIT;
	git_odb_backend_pack_options backend_opts = GIT_ODB_BACKEND_PACK_OPTIONS_INIT;

	odb_opts.oid_type = GIT_OID_SHA256;
	backend_opts.oid_type = GIT_OID_SHA256;

	cl_git_pass(git_odb_new(&_odb, &odb_opts));
	cl_git_pass(git_odb_backend_one_pack(&backend, cl_fixture("packfile-sha256/pack-b4a043c0ec5e079e8ac67d823776d752efc71661592db317474a0cf292915f31.idx"), &backend_opts));
	cl_git_pass(git_odb_add_backend(_odb, backend, 1));
#endif
}

void test_odb_packed_one_sha256__cleanup(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	git_odb_free(_odb);
	_odb = NULL;
#endif
}

void test_odb_packed_one_sha256__mass_read(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(packed_objects_one_sha256); ++i) {
		git_oid id;
		git_odb_object *obj;

		cl_git_pass(git_oid_fromstr(&id, packed_objects_one_sha256[i], GIT_OID_SHA256));
		cl_assert(git_odb_exists(_odb, &id) == 1);
		cl_git_pass(git_odb_read(&obj, _odb, &id));

		git_odb_object_free(obj);
	}
#endif
}

void test_odb_packed_one_sha256__read_header_0(void)
{
#ifdef GIT_EXPERIMENTAL_SHA256
	unsigned int i;

	for (i = 0; i < ARRAY_SIZE(packed_objects_one_sha256); ++i) {
		git_oid id;
		git_odb_object *obj;
		size_t len;
		git_object_t type;

		cl_git_pass(git_oid_fromstr(&id, packed_objects_one_sha256[i], GIT_OID_SHA256));

		cl_git_pass(git_odb_read(&obj, _odb, &id));
		cl_git_pass(git_odb_read_header(&len, &type, _odb, &id));

		cl_assert(obj->cached.size == len);
		cl_assert(obj->cached.type == type);

		git_odb_object_free(obj);
	}
#endif
}
