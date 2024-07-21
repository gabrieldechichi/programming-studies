
vertTable = function(id, size, figs, value)
{
  var table = "<table id='" + id + "'>";
  for (var i = 0; i < size; i++)
  {
    var val = value(i);
    if (val == 0)
    {
      table += "<tr><td class='both zero'>";
    }
    else
    {
      table += "<tr><td class='both'>";
    }

    var space = val < 0 ? "" : "&nbsp;";
    table += space + val.toFixed(figs);
    table += "</td></tr>";
  }
  table += "</table>"

  return table;
}

horizTable = function(id, size, figs, value)
{
  var table = "<table id='" + id + "'><tr>";
  for (var i = 0; i < size; i++)
  {
    var val = value(i);

    if (val == 0)
    {
      if (i == 0)
      {
        table += "<td class='zero left'>";
      }
      else if (i == size - 1)
      {
        table += "<td class='zero right'>";
      }
      else
      {
        table += "<td class='zero'>";
      }
    }
    else
    {
      if (i == 0)
      {
        table += "<td class='left'>";
      }
      else if (i == size - 1)
      {
        table += "<td class='right'>";
      }
      else
      {
        table += "<td>";
      }
    }

    var space = val < 0 ? "" : "&nbsp;";
    table += space + val.toFixed(figs) + "</td>";
  }
  table += "</tr></table>"

  return table;
}

fullTable = function(id, width, height, figs, value)
{
  var table = "<table id='" + id + "'>";
  for (var i = 0; i < height; i++)
  {
    table += "<tr>";
    for (var j = 0; j < width; j++)
    {
      var val = value(i, j);

      if (val == 0)
      {
        if (j == 0)
        {
          table += "<td class='zero left'>";
        }
        else if (j == width-1)
        {
          table += "<td class='zero right'>";
        }
        else
        {
          table += "<td class='zero'>";
        }
      }
      else
      {
        if (j == 0)
        {
          table += "<td class='left'>";
        }
        else if (j == width-1)
        {
          table += "<td class='right'>";
        }
        else
        {
          table += "<td>";
        }
      }

      var space = val < 0 ? "" : "&nbsp;";
      table += space + val.toFixed(figs) + "</td>";
    }
    table += "</tr>";
  }

  table += "</table>"

  return table;
}
