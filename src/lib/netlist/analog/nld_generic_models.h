// license:GPL-2.0+
// copyright-holders:Couriersud
/*
 * nl_generic_models.h
 *
 */

#ifndef NLD_GENERIC_MODELS_H_
#define NLD_GENERIC_MODELS_H_

#include "netlist/nl_base.h"
#include "netlist/nl_setup.h"

#include <cmath>

namespace netlist
{
namespace analog
{

	// -----------------------------------------------------------------------------
	// A generic capacitor model
	// -----------------------------------------------------------------------------

	enum class capacitor_e
	{
		VARIABLE_CAPACITY,
		CONSTANT_CAPACITY
	};

	template <capacitor_e TYPE>
	class generic_capacitor
	{
	};

	template <>
	class generic_capacitor<capacitor_e::VARIABLE_CAPACITY>
	{
	public:
		generic_capacitor(device_t &dev, const pstring &name)
		: m_h(dev, name + ".m_h", 0.0)
		, m_c(dev, name + ".m_c", 0.0)
		, m_v(dev, name + ".m_v", 0.0)
		, m_gmin(0.0)
		{
		}

		capacitor_e type() const { return capacitor_e::VARIABLE_CAPACITY; }

		nl_double G(nl_double cap) const
		{
			//return m_h * (2.0 * cap - m_c) +  m_gmin;
			return m_h * 0.5 * (cap + m_c) +  m_gmin;
		}

		nl_double Ieq(nl_double cap, nl_double v) const
		{
			plib::unused_var(v);
	//		return m_h * (cap * v - m_charge) - G(cap) * v;
			return -m_h * 0.5 * (cap + m_c) * m_v;
		}

		void timestep(nl_double cap, nl_double v, nl_double step)
		{
			m_h = 1.0 / step;
			m_c = cap;
			m_v = v;
		}

		void setparams(nl_double gmin) { m_gmin = gmin; }

	private:
		state_var<double> m_h;
		state_var<double> m_c;
		state_var<double> m_v;
		nl_double m_gmin;
	};

	template <>
	class generic_capacitor<capacitor_e::CONSTANT_CAPACITY>
	{
	public:
		generic_capacitor(device_t &dev, const pstring &name)
		: m_h(dev, name + ".m_h", 0.0)
		, m_gmin(0.0)
		{
		}

		capacitor_e type() const { return capacitor_e::CONSTANT_CAPACITY; }
		nl_double G(nl_double cap) const { return cap * m_h +  m_gmin; }
		nl_double Ieq(nl_double cap, nl_double v) const { return - G(cap) * v; }

		void timestep(nl_double cap, nl_double v, nl_double step)
		{
			plib::unused_var(cap, v);
			m_h = 1.0 / step;
		}
		void setparams(nl_double gmin) { m_gmin = gmin; }
	private:
		state_var<nl_double> m_h;
		nl_double m_gmin;
	};

	// -----------------------------------------------------------------------------
	// A generic diode model to be used in other devices (Diode, BJT ...)
	// -----------------------------------------------------------------------------

	enum class diode_e
	{
		BIPOLAR,
		MOS
	};

	template <diode_e TYPE>
	class generic_diode
	{
	public:
		generic_diode(device_t &dev, const pstring &name)
		: m_Vd(dev, name + ".m_Vd", 0.7)
		, m_Id(dev, name + ".m_Id", 0.0)
		, m_G(dev,  name + ".m_G", 1e-15)
		, m_Vt(0.0)
		, m_Vmin(0.0) // not used in MOS model
		, m_Is(0.0)
		, m_logIs(0.0)
		, m_n(0.0)
		, m_gmin(1e-15)
		, m_VtInv(0.0)
		, m_Vcrit(0.0)
		{
			set_param(1e-15, 1, 1e-15, 300.0);
		}

		void update_diode(const nl_double nVd)
		{
			nl_double IseVDVt(0.0);

			if (TYPE == diode_e::BIPOLAR && nVd < m_Vmin)
			{
				m_Vd = nVd;
				m_G = m_gmin;
				m_Id = - m_Is;
			}
			else if (TYPE == diode_e::MOS && nVd < constants::zero())
			{
				m_Vd = nVd;
				m_G = m_Is * m_VtInv + m_gmin;
				m_Id = m_G * m_Vd;
			}
			else if (/*TYPE == diode_e::MOS || */nVd < m_Vcrit)
			{
				m_Vd = nVd;
				IseVDVt = std::exp(std::min(300.0, m_logIs + m_Vd * m_VtInv));
				m_Id = IseVDVt - m_Is;
				m_G = IseVDVt * m_VtInv + m_gmin;
			}
			else
			{
				if (TYPE == diode_e::MOS && m_Vd < constants::zero())
					m_Vd = std::min(m_Vmin, nVd);

				const nl_double d = (nVd - m_Vd);
				const nl_double a = std::abs(nVd - m_Vd) * m_VtInv;
				m_Vd = m_Vd + (d < 0 ? -1.0 : 1.0) * std::log1p(a) * m_Vt;
				IseVDVt = std::exp(m_logIs + m_Vd * m_VtInv);
				//const double IseVDVt = m_Is * std::exp(m_Vd * m_VtInv);
				m_Id = IseVDVt - m_Is;
				m_G = IseVDVt * m_VtInv + m_gmin;
			}
		}

		void set_param(const nl_double Is, const nl_double n, nl_double gmin, nl_double temp)
		{
			m_Is = Is;
			m_logIs = std::log(Is);
			m_n = n;
			m_gmin = gmin;

			m_Vt = m_n * temp * constants::k_b() / constants::Q_e();

			m_Vmin = -5.0 * m_Vt;

			m_Vcrit = m_Vt * std::log(m_Vt / m_Is / constants::sqrt2());
			m_VtInv = constants::one() / m_Vt;
			//printf("%g %g\n", m_Vmin, m_Vcrit);
		}


		nl_double I() const { return m_Id; }
		nl_double G() const { return m_G; }
		nl_double Ieq() const { return (m_Id - m_Vd * m_G); }
		nl_double Vd() const { return m_Vd; }

		/* owning object must save those ... */

	private:
		state_var<nl_double> m_Vd;
		state_var<nl_double> m_Id;
		state_var<nl_double> m_G;

		nl_double m_Vt;
		nl_double m_Vmin;
		nl_double m_Is;
		nl_double m_logIs;
		nl_double m_n;
		nl_double m_gmin;

		nl_double m_VtInv;
		nl_double m_Vcrit;
	};


} // namespace analog
} // namespace netlist

#endif /* NLD_GENERIC_MODELS_H_ */
