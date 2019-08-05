
function factory.add_definitions(defs)
	factory.internal.add_definitions(defs)
end

function factory.define_command(products, inputs, arglist, tmpdirs)
	factory.internal.define_command(factory.listify(products),
	    factory.listify(inputs), factory.listify(arglist),
	    factory.listify(tmpdirs))
end

function factory.include_config(paths, config)
	factory.internal.include_config(factory.listify(paths), config)
end

function factory.include_script(paths, config)
	factory.internal.include_script(factory.listify(paths), config)
end

function factory.replace_ext(file, old, new)
	return string.gsub(file, "%." .. old .. "$", "." .. new)
end

function factory.array_concat(t1, t2)
	for _,v in ipairs(t2) do
		table.insert(t1, v)
	end
end

function factory.build_path(...)
	path = ""
	for _, v in ipairs{...} do
		if (v ~= '') then
			if (path == '') then
				path = v
			else
				path = path .. "/" .. v
			end
		end
	end

	return path
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

function factory.listify(arg)
	if type(arg) == 'table' then
		return arg
	else
		return {arg}
	end
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

function factory.pretty_print_str(o)
   if type(o) == 'table' then
      local s = '{ '
      for k,v in pairs(o) do
         if type(k) ~= 'number' then k = '"'..k..'"' end
         s = s .. '['..k..'] = ' .. factory.pretty_print_str(v) .. ','
      end
      return s .. '} '
   else
      return tostring(o)
   end
end
