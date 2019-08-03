
function factory.add_definitions(...)
	factory.internal.add_definitions(...)
end

function factory.define_command(products, inputs, arglist)
	factory.internal.define_command(products, inputs, arglist)
end

function factory.replace_ext(file, old, new)
	return string.gsub(file, "%." .. old .. "$", "." .. new)
end

function factory.array_concat(t1, t2)
	for _,v in ipairs(t2) do
		table.insert(t1, v)
	end
end

function factory.flat_list(...)
	list = {}

	for _, v in ipairs{...} do
		if (type(v) == 'table') then
			factory.array_concat(list, v)
		else
			table.insert(list, tostring(v))
		end
	end

	return list
end

function factory.map(func, list)
	result = {}

	for _, v in ipairs(list) do
		table.insert(result, func(v))
	end

	return result
end

function factory.addprefix(prefix, list)
	return factory.map(function (lib) return prefix .. lib end, list)
end

function dump(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. dump(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end


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
			read_paths = {
				"/usr/local/llvm80",
				"/usr/local/bin",
				"/lib",
				"/usr/lib",
				"/usr/local/lib",
			}

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
				read_paths,
				libpaths
			)

			factory.define_command(prog_path, inputs, arglist)
		end
	}
}

factory.add_definitions(definitions)
