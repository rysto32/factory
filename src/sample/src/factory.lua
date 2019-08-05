
objdirprefix = "/home/rstone/obj/tcplat"
libdir = objdirprefix .. "/lib"
bindir = objdirprefix .. "/bin"

function make_lib_path(lib)
	return factory.build_path(libdir, "lib" .. lib .. ".a")
end

function define_obj_create(dir)
	factory.define_command(dir, {}, {"/bin/mkdir", "-p", dir})
end

define_obj_create(objdirprefix)
define_obj_create(libdir)
define_obj_create(bindir)

include_path = "include"

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
				objpath = objdir .. "/" .. objname

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
					"/usr/local/llvm80",
					"/usr/local/bin",
					"/usr/include/",
					"/usr/share",
					"/etc",
					srcdir,
					srcpath,
					include_path
				}

				factory.define_command({objpath}, inputs, arglist, objdir)

				table.insert(objs, objpath)
			end

			arglist = { parent_config["AR"], "crs", libpath}
			factory.array_concat(arglist, objs)

			factory.define_command(libpath, objs, arglist)
		end,
	},
	{
		name = "program",
		process = function(parent_config, config)
			libpaths = factory.map(make_lib_path, config["libs"])
			stdlibs = factory.addprefix("-l", config["stdlibs"])

			prog_path = bindir .. "/" .. config["path"]
			arglist = factory.flat_list(
				parent_config["LD"],
				"-o", prog_path,
				libpaths,
				stdlibs
			)

			inputs = factory.flat_list(
				"/usr/local/llvm80",
				"/usr/local/bin",
				"/lib",
				"/usr/lib",
				"/usr/local/lib",
				"/usr/share",
				"/etc",
				"/usr/bin",
				libpaths
			)

			factory.define_command(prog_path, inputs, arglist, bindir)
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
	CXX = "/usr/local/bin/clang++80",
	LD = "/usr/local/bin/clang++80",
	srcdir = "",
}

factory.add_definitions(definitions)
factory.include_config("build.ucl", top_config)

