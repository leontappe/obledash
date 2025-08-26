/*
 * This program is free software; you can use it, redistribute it
 * and / or modify it under the terms of the GNU General Public License
 * (GPL) as published by the Free Software Foundation; either version 3
 * of the License or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 *  WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program, in a file called gpl.txt or license.txt.
 *  If not, write to the Free Software Foundation Inc.,
 *  59 Temple Place - Suite 330, Boston, MA  02111-1307 USA
 */

import { Component, OnInit, OnDestroy } from "@angular/core";
import { ApiService } from "../services/api.service";
import { WifiInfo, DTCs } from "../definitions";
import { interval, Subscription } from "rxjs";

interface DashboardData {
  rpm?: number;
  speed?: number;
  engineTemp?: number;
  fuelLevel?: number;
  batteryVoltage?: number;
  engineLoad?: number;
  timestamp?: Date;
}

@Component({
    selector: "ui-dashboard",
    templateUrl: "./dashboard.component.html",
    styleUrls: ["./dashboard.component.scss"],
    standalone: false
})
export class DashboardComponent implements OnInit, OnDestroy {

    version: string | undefined;
    wifiInfo: WifiInfo | undefined;
    dtcs: DTCs | undefined;
    dashboardData: DashboardData = {};
    isConnected: boolean = false;
    lastUpdate: Date | undefined;
    
    private updateSubscription: Subscription | undefined;

    constructor(private $api: ApiService) {
    }

    ngOnInit(): void {
        this.loadInitialData();
        this.startDataUpdates();
    }

    ngOnDestroy(): void {
        if (this.updateSubscription) {
            this.updateSubscription.unsubscribe();
        }
    }

    loadInitialData(): void {
        this.$api.version().subscribe(ver => this.version = ver);
        this.$api.wifiInfo().subscribe((wi) => this.wifiInfo = wi);
        this.$api.dtcs().subscribe((d: DTCs) => this.dtcs = d);
    }

    startDataUpdates(): void {
        // Update every 2 seconds for demo purposes
        // In a real scenario, this would fetch live OBD data
        this.updateSubscription = interval(2000).subscribe(() => {
            this.updateDashboardData();
        });
    }

    updateDashboardData(): void {
        // Simulate real-time OBD data updates
        // In a real implementation, this would call an API endpoint
        this.dashboardData = {
            rpm: Math.floor(Math.random() * 3000) + 800,
            speed: Math.floor(Math.random() * 120),
            engineTemp: Math.floor(Math.random() * 50) + 80,
            fuelLevel: Math.floor(Math.random() * 100),
            batteryVoltage: (Math.random() * 2 + 11).toFixed(1) as any,
            engineLoad: Math.floor(Math.random() * 100),
            timestamp: new Date()
        };
        
        this.isConnected = true;
        this.lastUpdate = new Date();
    }

    getRpmGaugePercentage(): number {
        return this.dashboardData.rpm ? (this.dashboardData.rpm / 6000) * 100 : 0;
    }

    getSpeedGaugePercentage(): number {
        return this.dashboardData.speed ? (this.dashboardData.speed / 200) * 100 : 0;
    }

    getEngineLoadPercentage(): number {
        return this.dashboardData.engineLoad || 0;
    }

    getFuelLevelPercentage(): number {
        return this.dashboardData.fuelLevel || 0;
    }

    getConnectionStatus(): string {
        return this.isConnected ? 'Connected' : 'Disconnected';
    }

    getConnectionClass(): string {
        return this.isConnected ? 'text-success' : 'text-danger';
    }

    getEngineStatus(): string {
        if (!this.dashboardData.rpm) return 'Unknown';
        return this.dashboardData.rpm > 800 ? 'Running' : 'Idle';
    }

}