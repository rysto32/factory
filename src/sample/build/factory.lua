

function make_lib_path(lib)
	return "/home/rstone/obj/tcplat/lib" .. lib .. ".a"
end

definitions = {
	{
		name = "library",
		process = function (parent_config, defs)
			libpath = make_lib_path(defs["name"])
			objdir = "/home/rstone/obj/tcplat/"

			factory.define_command(objdir, {}, {"/bin/mkdir", "-p", objdir})

			objs = {}
			for i, src in ipairs(defs["srcs"]) do
				srcdir="/home/rstone/src/tcplat/"
				srcpath=srcdir .. src

				objname = factory.replace_ext(src, "[a-zA-Z0-9]+", "o")
				objpath = objdir .. objname

				arglist = factory.flat_list(
					parent_config["CXX"],
					"-c",
					defs["cflags"],
					"-o", objpath,
					srcpath)

				inputs = {
					"/usr/local/llvm80",
					"/usr/local/bin",
					"/usr/include/",
					srcdir,
					srcpath
				}

				factory.define_command({objpath}, inputs, arglist)

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

			prog_path = config["path"]
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
				libpaths
			)

			factory.define_command(prog_path, inputs, arglist)
		end
	}
}

factory.add_definitions(definitions)
