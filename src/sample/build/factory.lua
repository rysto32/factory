
--[[
factory={}
test_data = {
	program = {
		path = "/tmp/tcplat/tcplat",
		libs = {
			"tcplat",
		},

		stdlibs = {
			"pthread",
		},
	},

	library = {
		name = "tcplat",
		srcs = {
			"HistoInfo.cpp",
			"KernelController.cpp",
			"MsgSocket.cpp",
			"RequestClient.cpp",
			"SlaveControlStrategy.cpp",
			"SlaveServer.cpp",
			"SocketThread.cpp",
			"tcplat.cpp",
			"TestMaster.cpp",
			"TestSlave.cpp",
			"Thread.cpp",
			"UserController.cpp"
		},
		cflags = {"-O2", "-g", "-Wall"}
	}
}  --]]

--[[
function factory.define_job (products, inputs, arglist)
	print("Run " .. dump(arglist) .. " on " .. dump(inputs) .. " to produce " .. dump(products))


end
 --]]

function factory.array_concat(t1, t2)
	for _,v in ipairs(t2) do
		table.insert(t1, v)
	end
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

--[[
function factory.add_definitions(definitions)
	for _, def in ipairs(definitions) do
		print (dump(def))

		if (def.name == nil) then
			print("Config ingest name not defined")
			exit(1)
		end

		if (def.process == nil) then
			print("No process method for ingesting '" .. def.name .. "' config")
			exit(1)
		end

		def.process({}, test_data[def.name])
	end
end
--]]

function make_lib_path(lib)
	return "/home/rstone/obj/tcplat/lib" .. lib .. ".a"
end

definitions = {
	{
		name = "library",
		process = function(parent_config, defs)
			libpath = make_lib_path(defs["name"])
			objdir = "/home/rstone/obj/tcplat/"

			factory.define_command({objdir}, {}, {"/bin/mkdir", "-p", objdir})

			objs = {}
			for i, src in ipairs(defs["srcs"]) do
				srcdir="/home/rstone/src/tcplat/"
				srcpath=srcdir .. src

				objname = string.gsub(src, "%.[a-zA-Z0-9]+$", ".o")
				objpath = objdir .. objname

				arglist = { parent_config["CXX"], "-c" }
				factory.array_concat(arglist, defs["cflags"])
				table.insert(arglist, "-o")
				table.insert(arglist, objpath)
				table.insert(arglist, srcpath)

				inputs = {
					rw = { objpath, objdir },
					ro = {
						"/usr/include/",
						"/usr/local/llvm80",
						"/usr/local/bin",
						"/lib",
						"/usr/lib",
						"/usr/local/lib",
						srcdir,
						srcpath
					},
				}

				factory.define_command({objpath}, inputs, arglist)

				table.insert(objs, objpath)
			end

			arglist = { parent_config["AR"], "crs", libpath}
			factory.array_concat(arglist, objs)

			inputs = {
				rw = {libpath, objdir},
				ro = objs
			}

			factory.define_command(libpath, inputs, arglist)
		end
	},
	{
		name = "program",
		process = function(parent_config, config)
			libpaths = {}
			read_paths = {
				"/usr/local/llvm80",
				"/usr/local/bin",
				"/lib",
				"/usr/lib",
				"/usr/local/lib",
			}

			for _, lib in ipairs(config["libs"]) do
				libpath = make_lib_path(lib)
				table.insert(libpaths, libpath)
				table.insert(read_paths, libpath)
			end

			stdlibs = {}
			for _, lib in ipairs(config["stdlibs"]) do
				table.insert(stdlibs, "-l" .. lib)
			end

			prog_path = config["path"]
			arglist = {parent_config["LD"], "-o", prog_path}
			factory.array_concat(arglist, libpaths)
			factory.array_concat(arglist, stdlibs)

			inputs = {
				rw = {prog_path},
				ro = read_paths
			}

			factory.define_command(prog_path, inputs, arglist)
		end
	}
}

factory.add_definitions(definitions)
