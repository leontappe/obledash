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

import { ComponentFixture, TestBed } from '@angular/core/testing';
import { HttpClientTestingModule } from '@angular/common/http/testing';
import { DashboardComponent } from './dashboard.component';
import { ApiService } from '../services/api.service';
import { of } from 'rxjs';

describe('DashboardComponent', () => {
  let component: DashboardComponent;
  let fixture: ComponentFixture<DashboardComponent>;
  let apiService: jasmine.SpyObj<ApiService>;

  beforeEach(async () => {
    const spy = jasmine.createSpyObj('ApiService', ['version', 'wifiInfo', 'dtcs']);

    await TestBed.configureTestingModule({
      declarations: [DashboardComponent],
      imports: [HttpClientTestingModule],
      providers: [
        { provide: ApiService, useValue: spy }
      ]
    }).compileComponents();

    fixture = TestBed.createComponent(DashboardComponent);
    component = fixture.componentInstance;
    apiService = TestBed.inject(ApiService) as jasmine.SpyObj<ApiService>;
  });

  it('should create', () => {
    // Setup API service mocks
    apiService.version.and.returnValue(of('1.0.0'));
    apiService.wifiInfo.and.returnValue(of({ SSID: 'TestWiFi', ip: '192.168.1.1', mac: '00:11:22:33:44:55' }));
    apiService.dtcs.and.returnValue(of({ dtc: ['P0195'] }));

    expect(component).toBeTruthy();
  });

  it('should load initial data on init', () => {
    // Setup API service mocks
    apiService.version.and.returnValue(of('1.0.0'));
    apiService.wifiInfo.and.returnValue(of({ SSID: 'TestWiFi', ip: '192.168.1.1', mac: '00:11:22:33:44:55' }));
    apiService.dtcs.and.returnValue(of({ dtc: ['P0195'] }));

    component.ngOnInit();

    expect(apiService.version).toHaveBeenCalled();
    expect(apiService.wifiInfo).toHaveBeenCalled(); 
    expect(apiService.dtcs).toHaveBeenCalled();
  });

  it('should calculate RPM gauge percentage correctly', () => {
    component.dashboardData = { rpm: 3000 };
    const percentage = component.getRpmGaugePercentage();
    expect(percentage).toBe(50); // 3000/6000 * 100
  });

  it('should calculate speed gauge percentage correctly', () => {
    component.dashboardData = { speed: 100 };
    const percentage = component.getSpeedGaugePercentage();
    expect(percentage).toBe(50); // 100/200 * 100
  });

  it('should return correct connection status', () => {
    component.isConnected = true;
    expect(component.getConnectionStatus()).toBe('Connected');
    
    component.isConnected = false;
    expect(component.getConnectionStatus()).toBe('Disconnected');
  });

  it('should return correct engine status', () => {
    component.dashboardData = { rpm: 1500 };
    expect(component.getEngineStatus()).toBe('Running');
    
    component.dashboardData = { rpm: 700 };
    expect(component.getEngineStatus()).toBe('Idle');
    
    component.dashboardData = {};
    expect(component.getEngineStatus()).toBe('Unknown');
  });

  it('should cleanup subscription on destroy', () => {
    component.ngOnInit();
    spyOn(component['updateSubscription']!, 'unsubscribe');
    
    component.ngOnDestroy();
    
    expect(component['updateSubscription']!.unsubscribe).toHaveBeenCalled();
  });
});