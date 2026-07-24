--[[
  Pandoc filter for the DABstar user manual.

  The chapter files link to each other with plain file names so that they work when the manual is
  browsed on GitHub. In the PDF all chapters are one document, so those links have to become
  internal ones. Everything else that points into the repository is turned into a GitHub URL,
  because a relative file path is useless in a PDF.

    40-ensemble-list.md            ->  #ensemble-list
    30-service-list.md#some-part   ->  #some-part
    ../../README.md                ->  <repo>/README.md
    ../FIG-Overview                ->  <repo>/doc/FIG-Overview

  The anchor of a chapter is derived from its file name: strip the leading number and the
  extension. This requires the file name slug to match the slug of the chapter heading
  (60-scope-and-spectrum.md holds "# Scope and Spectrum"), which is a rule of this manual.
]]

local REPO_URL = 'https://github.com/tomneda/DABstar/blob/main/'
local MANUAL_DIR = { 'doc', 'manual' }

-- Resolve a path given relative to doc/manual/ into a repository relative one.
local function resolve(path)
  local parts = {}
  for _, segment in ipairs(MANUAL_DIR) do
    parts[#parts + 1] = segment
  end
  for segment in path:gmatch('[^/]+') do
    if segment == '..' then
      if #parts > 0 then table.remove(parts) end
    elseif segment ~= '.' then
      parts[#parts + 1] = segment
    end
  end
  return table.concat(parts, '/')
end

function Link(link)
  local target = link.target

  -- absolute URLs, mail addresses and links within the current document stay as they are
  if target:match('^%a[%w+.%-]*:') or target:match('^#') or target:match('^//') then
    return nil
  end

  local path, fragment = target:match('^([^#]*)#?(.*)$')

  local chapter = path:match('^(%d+%-[%w%-]+)%.md$')
  if chapter then
    if fragment ~= '' then
      link.target = '#' .. fragment
    else
      link.target = '#' .. chapter:gsub('^%d+%-', '')
    end
    return link
  end

  link.target = REPO_URL .. resolve(path)
  if fragment ~= '' then
    link.target = link.target .. '#' .. fragment
  end
  return link
end
