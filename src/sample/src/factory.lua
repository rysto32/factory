
objdirprefix = "/tmp/obj/tcplat"
libdir = objdirprefix .. "/lib"
bindir = objdirprefix .. "/bin"

function make_lib_path(lib)
	return factory.build_path(libdir, "lib" .. lib .. ".a")
end

function define_obj_create(dir)
	factory.define_command(dir, {"/lib"}, {"/bin/mkdir", dir})
end

define_obj_create(objdirprefix)
define_obj_create(libdir)
define_obj_create(bindir)

include_path = factory.build_path('.', "include")

definitions = {
	{
		name = "library",
		process = function (parent_config, defs)
			libpath = make_lib_path(defs["name"])

			objdir = factory.build_path(objdirprefix, "objects", parent_config.srcdir)
			define_obj_create(objdir)

			objs = {}
			for i, src in ipairs(defs["srcs"]) do
				if (parent_config.srcdir == '') then
					srcdir = "."
				else
					srcdir = parent_config.srcdir
				end
				srcpath=factory.build_path(parent_config.srcdir, src)

				objname = factory.replace_ext(src, "[a-zA-Z0-9]+", "o")
				objpath = factory.build_path(objdir, objname)

				arglist = factory.flat_list(
					parent_config["CXX"],
					"-c",
					"-O2",
					"-Wall",
					"-g",
					"-o", objpath,
					"-I", include_path,
					srcpath)

				inputs = {
					"/usr/local/bin",
					"/usr/include/",
					"/usr/share",
					"/usr/bin",
					"/etc",
					"/lib",
					srcdir,
					srcpath,
					include_path
				}

				options = {
					statdirs = "/"
				}
				factory.define_command({objpath}, inputs, arglist, options)

				table.insert(objs, objpath)
			end

			arglist = { parent_config["AR"], "crs", libpath}
			factory.list_concat(arglist, objs)

			factory.define_command(libpath, objs, arglist)
		end,
	},
	{
		name = "program",
		process = function(parent_config, config)
			libpaths = factory.map(make_lib_path, config["libs"])
			stdlibs = factory.addprefix("-l", config["stdlibs"])

			prog_path = factory.build_path(bindir, config["path"])
			arglist = factory.flat_list(
				parent_config["LD"],
				"-o", prog_path,
				libpaths,
				stdlibs
			)

			inputs = factory.flat_list(
				"/libexec",
				"/usr/local/bin",
				"/lib",
				"/usr/lib",
				"/usr/local/lib",
				"/usr/share",
				"/etc",
				"/usr/bin",
				libpaths
			)

			options = {
				tmpdirs = bindir,
				statdirs = "/"
			}
			factory.define_command(prog_path, inputs, arglist, options)
		end
	},
	{
		name = "subdirs",
		process = function(parent_config, config)
			orig_srcdir = parent_config.srcdir
			for _, dir in ipairs(config) do
				parent_config.srcdir = factory.build_path(orig_srcdir, dir)
				path = factory.build_path(parent_config.srcdir, "build.ucl")

				factory.include_config(path, parent_config)
			end
		end
	},
}

top_config = {
	AR = "/usr/bin/ar",
	CXX = "/usr/bin/c++",
	LD = "/usr/bin/c++",
	srcdir = "",
}

factory.add_definitions(definitions)
factory.include_config("build.ucl", top_config)

