function make_lib_path(lib)
	return "/home/rstone/obj/tcplat/lib" .. lib .. ".a"
end

objdir = "/home/rstone/obj/tcplat"

function define_obj_create(dir)
	factory.define_command(dir, {}, {"/bin/mkdir", "-p", dir})
end

definitions = {
	{
		name = "library",
		process = function (parent_config, defs)
			libpath = make_lib_path(defs["name"])

			define_obj_create(objdir)

			objs = {}
			for i, src in ipairs(defs["srcs"]) do
				srcdir="/home/rstone/src/tcplat/"
				srcpath=srcdir .. src

				objname = factory.replace_ext(src, "[a-zA-Z0-9]+", "o")
				objpath = objdir .. "/" .. objname

				arglist = factory.flat_list(
					parent_config["CXX"],
					"-c",
					"-O2",
					"-Wall",
					"-g",
					"-o", objpath,
					srcpath)

				inputs = {
					"/usr/local/llvm80",
					"/usr/local/bin",
					"/usr/include/",
					"/usr/share",
					"/etc",
					srcdir,
					srcpath
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
			bindir = objdir .. "/bin"
			define_obj_create(bindir)

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
			pathlist = factory.map(function (dir) return dir .. "/build.ucl" end, config)
			factory.include_config(pathlist)
		end
	},
}

factory.add_definitions(definitions)
factory.include_config("build.ucl")

