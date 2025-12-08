#include <iostream>
#include <vector>
#include <map>     
#include<fstream>
#include<cmath>
#include <algorithm>
#include <iomanip>

using namespace std;

// ==========================================
// 1. DATA OBJECTS
// ==========================================

class SwapQuote {
    private:
        double _maturity;
        double _rate;
    public:
        SwapQuote(double m, double r) : _maturity(m), _rate(r){}
        double maturity() const { return _maturity;}
        double rate() const {return _rate;}
};

// ==========================================
// 2. THE CURVE OBJECT
// ==========================================

class ZeroCurve {
private:
    map<double, double> _curveData;

public:
    void addNode(double time, double rate) {
        _curveData[time] = rate;
    }

    double getZeroRate(double t) const {
        if (_curveData.empty()){
            return 0.0;
        }
        auto it = _curveData.lower_bound(t);

    

        // Case A: Extrapolation (t is after the last point)
        if (it == _curveData.end()){
            return _curveData.rbegin()->second;
        }

        // Case B: Exact match or Extrapolation (t is before/at first point)
        if (it == _curveData.begin()) {
            return it->second;
        }

        // Case C: Interpolation
        auto it_t2 = it;
        auto it_t1 = prev(it);

        auto t1 = it_t1->first;
        auto t2 = it_t2->first;
        auto r1 = it_t1->second;
        auto r2 = it_t2->second;

        double interpolatedRate = (r2-r1)/(t2-t1) * (t-t1) + r1;

        return interpolatedRate;
    }

    double getDiscountFactor(double t) const {
        double r = getZeroRate(t);
        return exp(-r * t);
    }
    double getMaxMaturity() const{
        return _curveData.empty() ? 0.0 : _curveData.rbegin()->first;
    }
    map<double,double> getCurve() const{
        return _curveData;
    }

};

// ==========================================
// 3. THE BOOTSTRAPPER 
// ==========================================

class Bootstrapper {
private:
    vector<SwapQuote> _quotes;
    const double FIXED_TAU = 0.5; // We assume semi-annual paiement 

public:

    Bootstrapper(const std::vector<SwapQuote>& quotes) : _quotes(quotes) {
        // Sort inputs by maturity to be safe
        std::sort(_quotes.begin(), _quotes.end(), 
             [](const SwapQuote& a, const SwapQuote& b) { 
                 return a.maturity() < b.maturity(); 
             });
    }

    ZeroCurve calibrate(ZeroCurve initialCurve) {
        ZeroCurve curve = initialCurve;
        for (const auto& swap : _quotes) {
            double mat = swap.maturity();
            double S = swap.rate();

            if (curve.getCurve().count(mat)){
                continue;
            }

            // Calculate the discount factor at DF_n = (1.0 - \sum_{i=1}^{n} \tau_i DF(T_i)) / (1.0 + tau_n*S)
            double sumDiscountedCoupons = 0.0;
            
            for(int i = 1; i*FIXED_TAU < mat; ++i) {
                sumDiscountedCoupons += S * FIXED_TAU * curve.getDiscountFactor(FIXED_TAU*i);
            }
  

            double tau_n = mat - floor(2.0*mat)/2.0; // The remaining tau_n for the last period [t_{n-1}, t_n[, t_n being the pillar

            
            double df_n = (1.0 - sumDiscountedCoupons)/(1.0 + tau_n*S);

            // Convert to Zero Rate (Continuously Compounded)
            // r_n = -ln(DF(T_n)) / t
            double zeroRate = (df_n > 0) ? -log(df_n) / mat : 0.0;

            curve.addNode(mat, zeroRate);
            
            cout << "Calibrated " << mat << "Y Swap. Zero Rate: " 
                      << (zeroRate * 100) << "%" << endl;
        }

        return curve;
        }



};

// ==========================================
// 4. SWAP PRICER (interpolation)
// ==========================================

class SwapPricer {
    private:
        const double FIXED_TAU = 0.5; // Semi-annual payments 
    public:

    // Calculates the Present Value of the Annuity (PV of all fixed coupons)
        double annuity(const ZeroCurve& curve, double maturity) const{
            double sum = 0.0;
            for(double t=FIXED_TAU; t< maturity ; t+= FIXED_TAU){
                sum+= FIXED_TAU * curve.getDiscountFactor(t);
            }
            //Handle the final period
            double last_tau = maturity - floor(2*maturity)/2.0; // Last irregular year fraction
            sum += last_tau * curve.getDiscountFactor(maturity);
            return sum;
        }

        // Calculates the Fair Swap Rate (S_fair) based on the curve
        double calculateFaireRate(const ZeroCurve& curve, double maturity) const{
            double A = annuity(curve, maturity);
            double DF_end = curve.getDiscountFactor(maturity);
            if (A<1e-8) return 0;
            return (1.0 - DF_end)/A;
        }

         // Prices a swap with a given fixed rate (Repricing / Verification)
        double priceSwap(const ZeroCurve& curve, double maturity, double fixedRate) const {
        // PV_Fixed = FixedRate * Annuity
        double pvFixed = fixedRate * annuity(curve, maturity);

        // PV_Float = 1.0 - DF(T_n)
        double pvFloat = 1.0 - curve.getDiscountFactor(maturity);

        // NPV = PV_Float - PV_Fixed (Receive Floating, Pay Fixed)
        return pvFloat - pvFixed; 
    }
};
// ==========================================
// 4. EXPORT FUNCTIONS
// ==========================================
void exportQuotes(const vector<SwapQuote>& quotes, const string& filename){
    ofstream file(filename);
    file << "Maturity,SwapRate" << endl;
    for (const auto& q : quotes){
        file << fixed << setprecision(8) << q.maturity() << "," << q.rate() << endl;
    }
    file.close();
    cout << "Swap quotes exported" << endl;
}

void exportCurve(const ZeroCurve& curve, const string& filename){
    ofstream file(filename);
    file << "Time,ZeroRate" << endl;

    const auto& curveData = curve.getCurve();

    if (curveData.empty()) return;
    
    for (const auto& pair: curveData){
        file << fixed << setprecision(8) << pair.first << "," << pair.second << endl;
    }
    file.close();
    cout << "Zero curve pillar exported" << endl;

}

// ==========================================
// 4. MAIN PROGRAM
// ==========================================

int main() {
    // 1. Setup Data
    double zcb_0_5_rate = 0.0100;
    vector<SwapQuote> marketData = {
        SwapQuote(0.5, zcb_0_5_rate), //Placeholder for ZCB
        SwapQuote(1.0, 0.0150),
        SwapQuote(2.0, 0.0190), 
        SwapQuote(3.0, 0.0240),
        SwapQuote(5.0, 0.0315),
        SwapQuote(6.0, 0.0400),
    };


    // INITIALIZE CURVE WITH ZCB 
    ZeroCurve curve;
    SwapPricer pricer;
    const double ZCB_TAU = 0.5;

    //Calculate DF(0.5) for ZCB rate
    double df_0_5 = 1.0/ (1.0 + zcb_0_5_rate*ZCB_TAU);
    double r_0_5 = -log(df_0_5) / ZCB_TAU;
    curve.addNode(ZCB_TAU, r_0_5); 

    cout << "--- Initialization (ZCB) ---" << endl;
    cout << fixed << setprecision(6) 
         << "Initial 0.5Y ZCB Rate: " << zcb_0_5_rate * 100 << "%"
         << " -> DF: " << df_0_5 
         << " -> Zero Rate: " << r_0_5 * 100 << "% (CC)" << endl;


    cout << "--- Boostrap ---" << endl;
    Bootstrapper solver(marketData);
    curve = solver.calibrate(curve);

    cout << "---Verification of the NPV---" <<endl;
    cout << setw(10) << "Maturity" << setw(15) << "Market Rate" << endl;
    for(const auto& q : marketData){
        double fairRate = pricer.calculateFaireRate(curve, q.maturity());
        double npv = pricer.priceSwap(curve,q.maturity(), q.rate());

        cout << fixed << setprecision(4)
             << setw(10) << q.maturity()
             << setw(15) << q.rate()*100 << "%"
             << setw(15)  << fairRate * 100 << "%"
             << " | NPV: " << npv << " (should be near 0)" << endl;
    }

    cout << "---Interpolation of new swaps---" << endl;
    vector<double> newSwapMaturities = {4.0,4.7,5.5};
    vector<SwapQuote> interpolatedSwaps;

    for(double mat: newSwapMaturities){
        double fairRateInterp = pricer.calculateFaireRate(curve, mat);
        SwapQuote interpolatedSwap(mat, fairRateInterp);
        interpolatedSwaps.push_back(interpolatedSwap);
    }


   // Export the curve for plotting
    exportQuotes(marketData, "swap_quotes.csv");
    exportQuotes(interpolatedSwaps, "interpolated_swaps.csv");
    exportCurve(curve, "zero_curve.csv");

    return 0;
}