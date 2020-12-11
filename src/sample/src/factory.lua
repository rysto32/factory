
objdirprefix = "/tmp/obj/tcplat"
libdir = objdirprefix .. "/lib"
bindir = objdirprefix .. "/bin"

function make_lib_path(lib)
	return factory.build_path(libdir, "lib" .. lib .. ".a")
end

my_vars = {
	MK_MASTER = "0"
}

function define_obj_create(dir)
	factory.define_command(dir, {'/lib', '/bin'}, {"/bin/mkdir", dir})
end

function get_var(defs, key)
	local val, val1

	val = defs[key]
	val1 = defs[key .. ".1"]
	if val and val1
	then
		return factory.list_concat(val, val1)
	elseif val
	then
		return val
	elseif val1
	then
		return val1
	else
		return {}
	end
end

define_obj_create(objdirprefix)
define_obj_create(libdir)
define_obj_create(bindir)

include_path = "include"

definitions = {
	{
		name = "library",
		process = function (parent_config, defs)
			libpath = make_lib_path(get_var(defs, "name"))

			objdir = factory.build_path(objdirprefix, "objects", parent_config.srcdir)
			define_obj_create(objdir)

			objs = {}
			for i, src in ipairs(get_var(defs, "srcs")) do
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
					"/usr/lib",
					"/lib",
					"/usr/bin",
					"/libexec",
					"/etc",
					srcdir,
					srcpath,
					include_path
				}

				options = {
					tmpdirs = objdir
				}
				factory.define_command({objpath}, inputs, arglist, options)

				table.insert(objs, objpath)
			end

			arglist = { parent_config["AR"], "crs", libpath}
			factory.list_concat(arglist, objs)

			local ar_inputs = factory.flat_list(
				'/usr/lib',
				'/lib',
				objs
			)

			factory.define_command(libpath, ar_inputs, arglist)
		end,
	},
	{
		name = "program",
		process = function(parent_config, config)
			local libpaths = factory.map(make_lib_path, get_var(config, "libs"))
			local stdlibs = factory.addprefix("-l", get_var(config, "stdlibs"))

			local prog_path = factory.build_path(bindir, get_var(config, "path"))
			local arglist = factory.flat_list(
				parent_config["LD"],
				"-o", prog_path,
				libpaths,
				stdlibs
			)

			local inputs = factory.flat_list(
				"/usr/local/bin",
				"/usr/share",
				"/usr/lib",
				"/lib",
				"/usr/bin",
				"/libexec",
				"/etc",
				libpaths
			)

			local options = {
				tmpdirs = bindir,
				targets = { 'programs' }
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

				factory.include_config(path, parent_config, my_vars)
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
factory.include_config("build.ucl", top_config, my_vars)

