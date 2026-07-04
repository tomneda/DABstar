//
// Created by tomneda on 2026-04-25.
//
#pragma once

#include <array>
#include <fstream>
#include <string>
#include <cmath>
#include <complex>

template <typename T, std::size_t N>
void plot_abs_array(const std::array<T, N>& iValues, const std::string& iTitle = "(no title)")
{
  static int plotCounter = 0;
  const std::string id = std::to_string(plotCounter++);
  const std::string dataFile = "/tmp/data_abs_" + id + ".dat";
  const std::string scriptFile = "/tmp/data_plot_" + id + ".gnuplot";

  {
    std::ofstream out(dataFile);
    for (std::size_t i = 0; i < iValues.size(); ++i)
    {
      out << i << " " << std::abs(iValues[i]) << '\n';
    }
  }

  {
    std::ofstream gp(scriptFile);
    gp << "set title '" << iTitle << "'\n";
    gp << "set xlabel 'Index'\n";
    gp << "set ylabel 'Magnitude'\n";
    gp << "set grid\n";
    gp << "plot '" << dataFile << "' using 1:2 with lines title 'Abs'\n";
  }

  std::system(("gnuplot -p " + scriptFile + " &").c_str());
}


template <typename T, std::size_t N>
void plot_real_imag_array(const std::array<T, N>& iValues, const std::string& iTitle = "(no title)")
{
  static int plotCounter = 0;
  const std::string id = std::to_string(plotCounter++);
  const std::string dataFile = "/tmp/data_real_imag_" + id + ".dat";
  const std::string scriptFile = "/tmp/data_plot_ri_" + id + ".gnuplot";

  {
    std::ofstream out(dataFile);
    for (std::size_t i = 0; i < iValues.size(); ++i)
    {
      out << i << " " << std::real(iValues[i]) << " " << std::imag(iValues[i]) << '\n';
    }
  }

  {
    std::ofstream gp(scriptFile);
    gp << "set title '" << iTitle << "'\n";
    gp << "set xlabel 'Index'\n";
    gp << "set ylabel 'Value'\n";
    gp << "set grid\n";
    gp << "plot '" << dataFile << "' using 1:2 with lines title 'Real', \\\n";
    gp << "     '" << dataFile << "' using 1:3 with lines title 'Imag'\n";
  }

  std::system(("gnuplot -p " + scriptFile + " &").c_str());
}

template <typename T, std::size_t N>
void plot_real_only_array(const std::array<T, N>& iValues, const std::string& iTitle = "(no title)")
{
  static int plotCounter = 0;
  const std::string id = std::to_string(plotCounter++);
  const std::string dataFile = "/tmp/data_real_only_" + id + ".dat";
  const std::string scriptFile = "/tmp/data_plot_ro_" + id + ".gnuplot";

  {
    std::ofstream out(dataFile);
    for (std::size_t i = 0; i < iValues.size(); ++i)
    {
      out << i << " " << std::real(iValues[i]) << '\n';
    }
  }

  {
    std::ofstream gp(scriptFile);
    gp << "set title '" << iTitle << "'\n";
    gp << "set xlabel 'Index'\n";
    gp << "set ylabel 'Value'\n";
    gp << "set grid\n";
    gp << "plot '" << dataFile << "' using 1:2 with lines title 'Real'\n";
  }

  std::system(("gnuplot -p " + scriptFile + " &").c_str());
}

template <typename T, std::size_t N>
void plot_imag_only_array(const std::array<T, N>& iValues, const std::string& iTitle = "(no title)")
{
  static int plotCounter = 0;
  const std::string id = std::to_string(plotCounter++);
  const std::string dataFile = "/tmp/data_imag_only_" + id + ".dat";
  const std::string scriptFile = "/tmp/data_plot_io_" + id + ".gnuplot";

  {
    std::ofstream out(dataFile);
    for (std::size_t i = 0; i < iValues.size(); ++i)
    {
      out << i << " " << std::imag(iValues[i]) << '\n';
    }
  }

  {
    std::ofstream gp(scriptFile);
    gp << "set title '" << iTitle << "'\n";
    gp << "set xlabel 'Index'\n";
    gp << "set ylabel 'Value'\n";
    gp << "set grid\n";
    gp << "plot '" << dataFile << "' using 1:2 with lines title 'Imag'\n";
  }

  std::system(("gnuplot -p " + scriptFile + " &").c_str());
}
