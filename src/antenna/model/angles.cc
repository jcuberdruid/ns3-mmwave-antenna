/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2011, 2012 CTTC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Nicola Baldo <nbaldo@cttc.es>
 */


#include <ns3/log.h>
#include <cmath>
#include "angles.h"


namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("Angles");

double
DegreesToRadians (double degrees)
{
  return degrees * M_PI / 180.0;

}

std::vector<double>
DegreesToRadians (const std::vector<double> &degrees)
{
  std::vector<double> radians;
  radians.reserve (degrees.size ());
  for (size_t i = 0; i < degrees.size () ; i++)
  {
    radians.push_back (DegreesToRadians (degrees[i]));
  }
  return radians;

}

double
RadiansToDegrees (double radians)
{
  return radians * 180.0 / M_PI;
}

std::vector<double>
RadiansToDegrees (const std::vector<double> &radians)
{
  std::vector<double> degrees;
  degrees.reserve (radians.size ());
  for (size_t i = 0; i < radians.size () ; i++)
  {
    degrees.push_back (RadiansToDegrees (radians[i]));
  }
  return degrees;
}

std::ostream& operator<< (std::ostream& os, const Angles& a)
{
  os << "(" << a.phi << ", " << a.theta << ")";
  return os;
}

std::istream &operator >> (std::istream &is, Angles &a)
{
  char c;
  is >> a.phi >> c >> a.theta;
  if (c != ':')
    {
      is.setstate (std::ios_base::failbit);
    }
  return is;
}


Angles::Angles ()
  : Angles (0, M_PI / 2)
{
}


Angles::Angles (double p, double t)
  : phi (p),
    theta (t)
{
  NormalizeAngles ();
}


Angles::Angles (Vector v)
  : Angles (std::atan2 (v.y, v.x),
            std::acos (v.z / v.GetLength ()))
{
}

Angles::Angles (Vector v, Vector o)
  : Angles (std::atan2 (v.y - o.y, v.x - o.x),
            std::acos ((v.z - o.z) / CalculateDistance (v, o)))
{
}

void Angles::NormalizeAngles ()
{
  this->phi = fmod (this->phi + M_PI, 2 * M_PI);
  if (this->phi < 0)
    {
      this->phi += M_PI;
    }
  else
    {
      this->phi -= M_PI;
    }
}

}

