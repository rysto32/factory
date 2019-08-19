
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
		return ''
	end

	local out = ''
	local i = 1
	while i <= #str do
		local c = str:sub(i,i)
		if c ~= '$' then
			out = out .. c
			i = i + 1
			goto continue
		end

		local next = str:sub(i+1,i+1)
		if next == '{' then
			local j = i + 1
			while j <= #str do
				next = str:sub(j, j)
				if next == '}' then
					varname = str:sub(i+2, j - 1)
					out = out .. factory.internal.expand_var(varname, vars)
					break
				end
				j = j + 1
			end

			i = j + 1
		else
			out = out .. factory.internal.expand_var(next, vars)
			i = i + 2
		end
		::continue::
	end

	return out
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

function factory.shell_split(str)
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
        if sep == nil then
                sep = "%s"
        end
        local t={}
        for str in string.gmatch(inputstr, "([^"..sep.."]+)") do
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
