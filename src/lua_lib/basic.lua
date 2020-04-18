
function factory.add_definitions(defs)
	factory.internal.add_definitions(defs)
end

function factory.basename(path)
	local _, ext
	_, _, base = path:find('([^/]+)$')
	return base
end

function factory.define_command(products, inputs, arglist, options)
-- 	print('products: ' .. factory.pretty_print_str(products))
-- 	print('inputs: ' .. factory.pretty_print_str(inputs))
-- 	print('arglist: ' .. factory.pretty_print_str(arglist))
-- 	print('options: ' .. factory.pretty_print_str(options))
-- 	print("")
	factory.internal.define_command(factory.listify(products),
	    factory.listify(inputs), factory.listify(arglist),
	    factory.listify(options))
end

function factory.define_mkdir(...)
	for _, d in ipairs{...} do
		factory.define_command(d, {"/bin", "/lib"}, {"mkdir", d}, {})
	end
end

function factory.include_config(paths, config, vars)
	factory.internal.include_config(factory.listify(paths), config, vars)
end

function factory.include_script(paths, config)
	factory.internal.include_script(factory.listify(paths), config)
end

function factory.file_ext(file)
	local _, ext
	_, _, ext = file:find('.+%.([^.]+)$')
	return ext
end

function factory.replace_ext(file, old, new)
	newfile = string.gsub(file, "%." .. old .. "$", "." .. new)
	return newfile
end

function factory.list_concat(t1, t2)
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

	-- XXX calling realpath here is the wrong thing to do
	return factory.realpath(path)
end

function factory.internal.expand_var(name, vars)

	if not vars[name] then
		print("Undefined variable '" .. name .. "'")
		os.exit(1)
	end

	--print("Var " .. name .. " -> " .. vars[name])
	return vars[name]
end

function factory.evaluate_vars(str, vars)
	if str == nil then
		return nil
	end

	return factory.internal.evaluate_vars(str, vars)
end

function factory.internal.eval_if(list, vars)

	if #list == 5 then
		if list[4] ~= 'else' then
			print("Malformed if expr: 4th component must be 'else' not '" .. list[4] .. "'")
		end
	elseif #list ~= 3 then
		print("Malformed if expr: should have 3 or 5 components")
		os.exit(1)
	end

	local condVal = factory.internal.eval_expr(list[2], vars)
	local cond

	print("Cond: " .. condVal)

	if condVal == "true" or condVal == "1" then
		cond = True
	elseif condVal == "false" or condVal == "0" then
		cond = False
	else
		print("if expr: condition must be a boolean")
		os.exit(1)
	end

	if cond then
		return factory.internal.eval_expr(list[3], vars)
	elseif #list == 5 then
		return factory.internal.eval_expr(list[5], vars)
	else
		return {}
	end
end

function factory.internal.eval_expr(list, vars)
	if type(list) == 'string' then
		return factory.internal.evaluate_vars(list, vars)
	end

	if type(list) ~= 'table' then
		print("Malformed expr: expected a list")
	end

	if type(list[1]) ~= 'string' then
		printf("Malformed expr: first element must be a string")
	end

	if list[1] == 'if' then
		return factory.internal.eval_if(list, vars)
	elseif #list == 1 then
		return factory.internal.evaluate_vars(list[1], vars)
	else
		print("Undefined expr type '" .. list[1] .. "' (size " .. #list .. ")")
		os.exit(1)
	end
end

function factory.eval_expr(list, vars)
	if type(list) == 'string' then
		return {list}
	end

	return factory.internal.eval_expr(list, vars)
end

function factory.flat_list(...)
	list = {}

	for _, v in ipairs{...} do
		if (type(v) == 'table') then
			factory.list_concat(list, v)
		elseif (v ~= nil) then
			table.insert(list, tostring(v))
		end
	end

	return list
end

function factory.listify(arg)
	if type(arg) == 'table' then
		return arg
	elseif arg == nil then
		return {}
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

function factory.realpath(path)
	return factory.internal.realpath(path)
end

function factory.shell_split(str)
	if str == nil then
		return nil
	end

	local out = {}
	local cur = ''
	local explicit_empty = false
	local quote = nil
	local i = 1
	while i <= #str do
		local c = str:sub(i,i)
		if not quote and c:find('%s') then
			if cur ~= '' or explicit_empty then
				table.insert(out, cur)
				cur = ''
				explicit_empty = false
			end
			goto continue
		end

		if c == quote then
			quote = nil
			goto continue
		end

		if not quote and (c == '"' or c == "'") then
			quote = c
			explicit_empty = true
			goto continue
		end

		cur = cur .. c
		explicit_empty = false

		::continue::
		i = i + 1
	end

	if cur ~= '' or explicit_empty then
		table.insert(out, cur)
	end

	return out
end

function factory.split(inputstr, sep)
	if inputstr == nil then
		return nil
	end

	sep = sep or "%s"
	local t={}
	local str
	for str in inputstr:gmatch("([^"..sep.."]+)") do
		table.insert(t, str)
	end
	return t
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
