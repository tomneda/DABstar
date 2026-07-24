--[[
  Pandoc filter for the DABstar user manual.

  The chapter files use plain Markdown images without any size attributes, because GitHub does
  not understand pandoc's "{width=50%}" syntax and would show it as literal text. The sizes for
  the PDF are therefore kept here.

  Screenshots are placed with a percentage of the text width. The default is the full text width;
  anything that is higher than it is wide needs a smaller value, otherwise the figure does not fit
  on the page. Rule of thumb for A4 with the margins set in pandoc.yaml: the text block is about
  16 cm wide and 22 cm high, so

      width in percent  <=  110 * (image width / image height)

  keeps a figure within the page. The small button icons are not scaled by width but set to the
  height of the surrounding text, so that they can be used inside a sentence.

  A size given in the Markdown file still wins, so a single figure can be overridden there if
  ever needed.
]]

local ICON_HEIGHT = '1.1em'
local DEFAULT_WIDTH = 100

-- Button icons shown inline within the text.
local ICONS = {
  ['device-button.png'] = true,
  ['favorite-button.png'] = true,
  ['scope-button.png'] = true,
  ['target-button.png'] = true,
  ['updown-buttons.png'] = true,
}

-- Screenshots that must not use the full text width, in percent of it.
local WIDTHS = {
  ['device-hackrf.png'] = 60,
  ['device-rtlsdr.png'] = 60,
  ['device-sdrplay.png'] = 60,
  ['journaline.png'] = 85,
  ['mainwidget.png'] = 85,
  ['service-selector.png'] = 45,
  ['tech-details.png'] = 45,
  ['update.png'] = 70,
}

function Image(image)
  -- do not touch a size that was given explicitly in the Markdown file
  if image.attributes.width or image.attributes.height then
    return nil
  end

  local name = image.src:match('([^/]+)$') or image.src

  if ICONS[name] then
    image.attributes.height = ICON_HEIGHT
  else
    image.attributes.width = (WIDTHS[name] or DEFAULT_WIDTH) .. '%'
  end

  return image
end
